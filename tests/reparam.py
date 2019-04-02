# coding: utf8
# Copyright © Scruge 2019.
# This file is part of ScrugeX.
# Created by Yaroslav Erohin.

import unittest
from eosfactory.eosf import *
from methods import *
from time import sleep
import string

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
		SCENARIO("Test cdp reparametrization")
		reset()

		create_master_account("master")

		create_account("eosio_token", master, "eosio.token")

		key = CreateKey(is_verbose=False)
		create_account("buck", master, "buck", key)
		perm(buck, key)

		deploy(Contract(eosio_token, "02_eosio_token"))
		deploy(Contract(buck, "eos-bucks/src"))

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

	def test(self):
		init(buck)
		update(buck)

		open(buck, user1, 2.0, 0) # cdp 0
		transfer(eosio_token, user1, buck, "100.0000 EOS", "")

		open(buck, user2, 2.0, 0) # cdp 1
		transfer(eosio_token, user2, buck, "99.0000 EOS", "")

		# test acr change
		changeacr(buck, user1, 0, 1.6)
		self.assertAlmostEqual(1.6, float(table(buck, "cdp", element="acr")))

		changeacr(buck, user1, 0, 0)
		self.assertAlmostEqual(0, float(table(buck, "cdp", element="acr")))


		# check starting 100 eos
		self.assertEqual(100, amount(table(buck, "cdp", element="collateral")))

		## + collateral
		reparam(buck, user1, 0, "0.0000 BUCK", "50.0000 EOS")

		assertRaisesMessage(self, "request already exists", 
			lambda: reparam(buck, user1, 0, "0.0000 BUCK", "50.0000 EOS"))
		
		self.assertEqual(0, table(buck, "reparamreq", element="isPaid"))

		# check does not work before collateral transfer
		self.assertEqual(100, amount(table(buck, "cdp", element="collateral")))

		# transfer collateral
		transfer(eosio_token, user1, buck, "50.0000 EOS", "r")

		self.assertEqual(1, table(buck, "reparamreq", element="isPaid"))

		# check nothing changes before update
		self.assertEqual(100, amount(table(buck, "cdp", element="collateral")))

		update(buck)

		# check change to 150 eos
		self.assertEqual(150, amount(table(buck, "cdp", element="collateral")))

		## - collateral
		reparam(buck, user1, 0, "0.0000 BUCK", "-50.0000 EOS")
		update(buck)

		# check change to 100 eos
		self.assertEqual(100, amount(table(buck, "cdp", element="collateral")))

		# check 50 eos came back to user1
		self.assertEqual(50, balance(eosio_token, user1))

		# check 99.5 buck of balance  to-do might change later
		self.assertEqual(99.5, balance(buck, user1))
		self.assertEqual(99.5, amount(table(buck, "cdp", element="debt")))

		## + debt
		reparam(buck, user1, 0, "10.5000 BUCK", "0.0000 EOS")
		update(buck)

		# check updated buck balance
		self.assertEqual(110, balance(buck, user1))
		self.assertEqual(110, amount(table(buck, "cdp", element="debt")))

		## - debt
		reparam(buck, user1, 0, "-10.0000 BUCK", "0.0000 EOS")

		# check instantly updated balance
		self.assertEqual(100, balance(buck, user1))

		run(buck)

		# check not yet updated cdp
		self.assertEqual(110, amount(table(buck, "cdp", element="debt")))

		update(buck)

		# check previously updated balance
		self.assertEqual(100, balance(buck, user1))

		# check now updated cdp debt after request complete
		self.assertEqual(100, amount(table(buck, "cdp", element="debt")))

		## reparam both at the same time

		# first create unpaid request
		reparam(buck, user2, 1, "0.0000 BUCK", "10.0000 EOS")

		# create actual request
		reparam(buck, user1, 0, "-10.0000 BUCK", "-10.0000 EOS")

		update(buck)

		self.assertEqual(90, amount(table(buck, "cdp", element="collateral")))
		self.assertEqual(90, amount(table(buck, "cdp", element="debt")))

		# check 10 eos came back
		self.assertEqual(90, balance(buck, user1))

		# check buck
		self.assertEqual(90, balance(buck, user1))





# main

if __name__ == "__main__":
	unittest.main()