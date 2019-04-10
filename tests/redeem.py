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
		SCENARIO("Test buck redeem")
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
		transfer(eosio_token, master, user1, "1000.0000 EOS", "")

	def run(self, result=None):
		super().run(result)

	# tests

	def test(self):
		SCENARIO("Test buck redeem")
		update(buck)

		open(buck, user1, 2.0, 0) # cdp 0
		transfer(eosio_token, user1, buck, "100.0000 EOS", "")

		sleep(2)
		update(buck)

		open(buck, user1, 2.1, 0) # cdp 1
		transfer(eosio_token, user1, buck, "101.0000 EOS", "")

		sleep(2)
		update(buck)

		balance(eosio_token, buck)

		# give some bucks to user 2
		transfer(buck, user1, user2, "150.0000 BUCK")

		table(buck, "cdp")

		## redeem from 1 cdp

		redeem(buck, user2, "10.0000 BUCK")

		sleep(2)
		update(buck)

		# sell from cdp 0: 		497.5100 REX
		# get from rex:		 	5.0248 EOS
		# add back to cdp 0:	0.0497 EOS
		# cdp 0 should be: 		95.0249 EOS

		# calculated leftover collateral for cdp 0
		self.assertEqual(95.0249, amount(table(buck, "cdp", element="collateral")))

		# check redeem request gone
		self.assertEqual(0, len(table(buck, "redeemreq")))

		# check balance taken
		self.assertAlmostEqual(140, balance(buck, user2))

		# value calculated with formula
		self.assertAlmostEqual(4.9751, balance(eosio_token, user2))

		table(buck, "cdp")



		## redeem from 2 cdp

		redeem(buck, user2, "100.0000 BUCK")

		sleep(2)
		update(buck)



		# total collateral:		49.7512 EOS
		# gained collateral:	50.2487 EOS
		# dividends: 			0.4975 EOS

		# dividends for 1:		47.8559 * 0.4975 / 49.7512		= 0.4785 EOS
		# dividends for 2:		1.8953 * 0.4975 / 49.7512		= 0.0189 EOS

		# initial collateral	101 BUCK
		# using debt: 			96.1904 BUCK
		# using collateral:		47.8559 EOS
		# new collateral:		53.1441 EOS
		# sell from cdp 1:		4785.5900 REX
		# get from rex:			48.3345 EOS
		# add back to cdp:		0.4786 EOS
		# cdp 1 should be: 		53.6227 EOS

		# initial collateral	95.0249
		# using debt:			3.8096 BUCK
		# using collateral:		1.8953 EOS
		# new collateral: 		93.1296 (was 95.0249)
		# sell rex:				189.5300 REX
		# get from rex:			0.0191 EOS
		# add back to cdp:		0.0001 EOS
		# cdp 0 should be: 		93.1296 EOS
		# 95.0249 - 1.8953 + 0.0189 = 93.1485

		# TO-DO: CALCULATE COLLATERAL CHANGE FOR CDPS IN REDEMPTION

		self.assertAlmostEqual(40, balance(buck, user2))

		# value calculated with formula
		# 49.7512 + 4.9751 = 54.7263
		self.assertAlmostEqual(54.7263, balance(eosio_token, user2))

		# check rex dividends added to cdp 0
		table(buck, "cdp")

		table(buck, "redprocess")



		## test if not enough debtors




# main

if __name__ == "__main__":
	unittest.main()