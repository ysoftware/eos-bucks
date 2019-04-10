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

		# give some bucks to user 2
		transfer(buck, user1, user2, "150.0000 BUCK")

		# redeem 10 bucks
		redeem(buck, user2, "10.0000 BUCK")

		sleep(2)
		update(buck)

		# check redeem request gone
		self.assertEqual(0, len(table(buck, "redeemreq")))

		# check balance taken
		self.assertAlmostEqual(140, balance(buck, user2))

		# value calculated with formula
		self.assertAlmostEqual(4.9751, balance(eosio_token, user2))

		table(buck, "cdp")

		# redeem 100 bucks
		redeem(buck, user2, "100.0000 BUCK")

		sleep(2)
		update(buck)

		self.assertAlmostEqual(40, balance(buck, user2))

		# value calculated with formula
		# 49.7512 + 4.9751 = 54.7263
		self.assertAlmostEqual(54.7263, balance(eosio_token, user2))

		# check rex dividends added to cdp 0
		table(buck, "cdp")





# main

if __name__ == "__main__":
	unittest.main()