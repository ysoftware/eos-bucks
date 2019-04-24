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

		open(buck, user1, 2.0, 0, "100.0000 EOS", eosio_token) # cdp 0 # rex.eos = 1000

		sleep(2)
		update(buck)

		open(buck, user1, 2.1, 0, "101.0000 EOS", eosio_token) # cdp 1 # rex.eos = 999

		sleep(2)
		update(buck)

		balance(eosio_token, buck)

		# give some bucks to user 2
		transfer(buck, user1, user2, "150.0000 BUCK")

		table(buck, "cdp")

		## redeem from 1 cdp

		redeem(buck, user2, "10.0000 BUCK")

		sleep(2)
		update(buck) # rex.eos = 998

		table(rex, "rexstat")

		# collateral 		100
		# using col:		4.9752 # debt / (price + rf)
		# gained			5.0350 # (col * 1.01 / rex.eos)
		# dividends			0.0598

		table(buck, "redprocess")

		# calculated leftover collateral for cdp 0
		self.assertEqual(95.0248, amount(table(buck, "cdp", element="collateral")))

		self.assertEqual(0.0598, amount(table(buck, "cdp", element="rex_dividends")))

		# check redeem request gone
		self.assertEqual(0, len(table(buck, "redeemreq")))

		# check balance taken
		self.assertAlmostEqual(140, balance(buck, user2))

		# value calculated with formula
		self.assertAlmostEqual(4.9752, balance(eosio_token, user2))

		table(buck, "cdp")

		## redeem from 2 cdp

		redeem(buck, user2, "100.0000 BUCK")

		sleep(2)
		update(buck) # rex.eos = 997

		self.assertAlmostEqual(40, balance(buck, user2))

		# value calculated with formula
		# 49.7514 + 4.9752 = 54.7266
		self.assertAlmostEqual(54.7266, balance(eosio_token, user2))

		# check rex dividends
		table(buck, "cdp")

		# match all other values



		## test if not enough debtors



		## test rounding 



# main

if __name__ == "__main__":
	unittest.main()