# coding: utf8
# Copyright © Scruge 2019.
# This file is part of ScrugeX.
# Created by Yaroslav Erohin.

import unittest
from eosfactory.eosf import *
from methods import *
from time import sleep
import string
import datetime

verbosity([Verbosity.INFO, Verbosity.OUT, Verbosity.TRACE, Verbosity.DEBUG])

CONTRACT_WORKSPACE = "eos-buck"

# methods

class Test(unittest.TestCase):

	# setup

	@classmethod
	def tearDownClass(cls):
		stop()

	@classmethod
	def setUpClass(cls):
		reset()

		create_master_account("master")
		create_account("eosio_token", master, "eosio.token")
		create_account("rex", master, "rexrexrexrex")
		
		key = CreateKey(is_verbose=False)
		create_account("buck", master, "buck", key)
		perm(buck, key)

		deploy(Contract(eosio_token, "eosio_token"))
		deploy(Contract(buck, "eos-bucks/src"))
		deploy(Contract(rex, "eos-bucks/tests/rexmock"))

		# Distribute tokens

		create_issue(eosio_token, master, "EOS")

		# Users

		create_account("user1", master, "user1")
		create_account("user2", master, "user2")

		transfer(eosio_token, master, user1, "150.0000 EOS", "")
		transfer(eosio_token, master, user2, "99.0000 EOS", "")

	def run(self, result=None):
		super().run(result)

	# tests

	### check rex values when selling rex

	def test(self):
		SCENARIO("Test cdp reparametrization")

		update(buck)

		# rex gives dividents, mock contract doesn't.
		# we need to have spare eos on contract to ensure proper workflow
		# when taking out collateral
		transfer(eosio_token, master, buck, "1000.0000 EOS", "rex fund") 

		open(buck, user1, 2.0, 0) # cdp 0
		transfer(eosio_token, user1, buck, "100.0000 EOS", "")

		open(buck, user2, 2.0, 0) # cdp 1
		transfer(eosio_token, user2, buck, "99.0000 EOS", "")

		sleep(2)
		update(buck)

		sleep(2)
		update(buck)

		# test acr change
		changeacr(buck, user1, 0, 1.6)
		self.assertAlmostEqual(1.6, float(table(buck, "cdp", element="acr")))

		changeacr(buck, user1, 0, 0)
		self.assertAlmostEqual(0, float(table(buck, "cdp", element="acr")))

		# check starting 100 eos
		self.assertEqual(100, amount(table(buck, "cdp", element="collateral")))

		# check starting 100 debt
		self.assertEqual(100, amount(table(buck, "cdp", element="debt")))




		## + collateral

		reparam(buck, user1, 0, "0.000 BUCK", "50.0000 EOS")

		self.assertEqual(0, table(buck, "reparamreq", element="isPaid"))

		# check does not work before collateral transfer
		self.assertEqual(100, amount(table(buck, "cdp", element="collateral")))

		# transfer collateral
		transfer(eosio_token, user1, buck, "50.0000 EOS", "r")

		self.assertEqual(1, table(buck, "reparamreq", element="isPaid"))

		# check nothing changes before update
		self.assertEqual(100, amount(table(buck, "cdp", element="collateral")))

		sleep(2)
		update(buck)

		sleep(3)
		update(buck)

		sleep(3)
		update(buck)

		# check requests were removed
		self.assertEqual(0, len(table(buck, "maturityreq")))
		self.assertEqual(0, len(table(buck, "reparamreq")))

		# check change to 150 eos
		self.assertEqual(150, amount(table(buck, "cdp", element="collateral")))
		
		# check 100 debt did not change
		self.assertEqual(100, amount(table(buck, "cdp", element="debt")))





		## - collateral

		balance(eosio_token, user1)
		balance(buck, user1)

		reparam(buck, user1, 0, "0.000 BUCK", "-50.0000 EOS")

		sleep(2)
		update(buck) # rex.eos = 997

		# check requests were removed
		self.assertEqual(0, len(table(buck, "maturityreq")))
		self.assertEqual(0, len(table(buck, "reparamreq")))

		# check change collateral to 100 eos
		self.assertEqual(100, amount(table(buck, "cdp", element="collateral")))

		# check 50.5843 eos came back to user1 (with rex dividends)
		self.assertEqual(50.6181, balance(eosio_token, user1))

		# check 100 debt did not change
		self.assertEqual(100, amount(table(buck, "cdp", element="debt")))





		## + debt

		reparam(buck, user1, 0, "10.000 BUCK", "0.0000 EOS")

		sleep(2)
		update(buck)

		# check requests were removed
		self.assertEqual(0, len(table(buck, "maturityreq")))
		self.assertEqual(0, len(table(buck, "reparamreq")))

		# check updated buck balance 98 + 9.8
		self.assertEqual(107.8, balance(buck, user1))

		# check updated debt 100 + 10
		self.assertEqual(110, amount(table(buck, "cdp", element="debt")))





		## - debt

		reparam(buck, user1, 0, "-10.000 BUCK", "0.0000 EOS")

		# check instantly updated balance
		self.assertEqual(97.8, balance(buck, user1))

		run(buck)

		# check not yet updated cdp
		self.assertEqual(110, amount(table(buck, "cdp", element="debt")))

		sleep(2)
		update(buck)

		# check requests were removed
		self.assertEqual(0, len(table(buck, "maturityreq")))
		self.assertEqual(0, len(table(buck, "reparamreq")))

		# check previously updated balance
		self.assertEqual(97.8, balance(buck, user1))

		# check now updated cdp debt after request complete
		self.assertEqual(100, amount(table(buck, "cdp", element="debt")))






		## reparam removing collateral and changing debt
		## also having an unpaid request

		# first create unpaid request
		reparam(buck, user2, 1, "0.000 BUCK", "99.0000 EOS")

		# create actual request
		reparam(buck, user1, 0, "-10.000 BUCK", "-10.0000 EOS")

		sleep(2)
		update(buck)

		# check requests were removed (1 left from unpaid request)
		self.assertEqual(5, len(table(buck, "maturityreq")))
		self.assertEqual(5, len(table(buck, "reparamreq")))

		self.assertEqual(90, amount(table(buck, "cdp", element="collateral")))
		self.assertEqual(90, amount(table(buck, "cdp", element="debt")))

		# check 10.1 eos came back to user1 (with rex dividends)
		self.assertEqual(60.7519, balance(eosio_token, user1))

		# check buck balance (97.8 - 10)
		self.assertEqual(87.8, balance(buck, user1))

		# verify did not touch unpaid request






		## reparam adding collateral and changing debt

		# create request
		reparam(buck, user1, 0, "-10.000 BUCK", "10.0000 EOS")

		# transfer collateral
		transfer(eosio_token, user1, buck, "10.0000 EOS", "r")	

		sleep(5)
		update(buck)

		sleep(5)
		update(buck)

		sleep(5)
		update(buck)

		print(datetime.datetime.now())	

		table(buck, "maturityreq")

		self.assertEqual(100, amount(table(buck, "cdp", element="collateral")))
		self.assertEqual(80, amount(table(buck, "cdp", element="debt")))

		# check buck balance (87.8 - 10)
		self.assertEqual(77.8, balance(buck, user1))

		# check requests were removed (1 left from unpaid request)
		self.assertEqual(5, len(table(buck, "maturityreq")))
		self.assertEqual(5, len(table(buck, "reparamreq")))


# main

if __name__ == "__main__":
	unittest.main()