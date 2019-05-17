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
		SCENARIO("Testing redemption")

		##############################
		COMMENT("Initialize")

		time = 0
		maketime(buck, time)
		update(buck)

		transfer(eosio_token, user1, buck, "20.0000 EOS", "deposit")

		time += 3_000_000
		maketime(buck, time)
		update(buck)

		open(buck, user1, 200, 0, "10000.0000 REX")
		open(buck, user1, 200, 0, "10000.0000 REX")

		##############################
		COMMENT("Redeem")

		balance(buck, user1)

		transfer(buck, user1, user2, "12500.0000 BUCK")

		# remove left over money
		transfer(buck, user1, master, balance(buck, user1, False))

		redeem(buck, user2, "500.0000 BUCK")

		time += 1
		maketime(buck, time)
		update(buck)

		##############################
		COMMENT("Match")

		# use col: debt / (price + rf)

		# collateral 		10000.0000
		# using deb:		500.0000
		# using col:		248.7562

		# calculated leftover collateral for cdp 0
		self.assertEqual(9751.2438, amount(table(buck, "cdp", row=0, element="collateral")))
		self.assertEqual(9500, amount(table(buck, "cdp", row=0, element="debt")))

		# check redeem request gone
		self.assertEqual(0, len(table(buck, "redeemreq")))

		# check balance taken
		self.assertAlmostEqual(12000, balance(buck, user2))

		# value calculated with formula
		self.assertAlmostEqual(248.7562, fundbalance(buck, user2))

		##############################
		COMMENT("Redeem")

		redeem(buck, user2, "12000.0000 BUCK")

		# collateral 		9751.2438
		# using deb:		9500.0000
		# using col:		4726.3681
		# return to debtor:	5024.8757

		# collateral 		10000.0000
		# using deb:		2500.0000
		# using col:		1243.7810
		# left col:			8756.2190
		# left deb:			7500.0000

		time += 1
		maketime(buck, time)
		update(buck)

		# check 1 cdp left
		self.assertEqual(1, len(table(buck, "cdp", row=None)))

		self.assertEqual(8756.2190, amount(table(buck, "cdp", row=0, element="collateral")))
		self.assertEqual(7500.0000, amount(table(buck, "cdp", row=0, element="debt")))

		# check redeem request gone
		self.assertEqual(0, len(table(buck, "redeemreq")))

		# check balance 
		self.assertAlmostEqual(0, balance(buck, user2))

		# value calculated with formula
		self.assertAlmostEqual(6218.9053, fundbalance(buck, user2))

		# value calculated with formula
		self.assertAlmostEqual(5024.8757, fundbalance(buck, user1))


# main

if __name__ == "__main__":
	unittest.main()