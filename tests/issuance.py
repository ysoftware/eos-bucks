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
		transfer(eosio_token, master, user1, "100.0000 EOS", "")

	def run(self, result=None):
		super().run(result)

	# tests

	def test(self):
		SCENARIO("Test init and open cdp")

		# initialize
		update(buck)

		# create cdp, transfer collateral
		open(buck, user1, 1.6, 0, "100.0000 EOS", eosio_token)

		# maturity
		sleep(2)

		# oracle update
		update(buck)

		# check rex
		self.assertEqual(0, amount(table(rex, "rexfund", element="balance")))
		self.assertEqual(100000, amount(table(rex, "rexbal", element="rex_balance")))

		# issuance formula
		price = 2
		col = 100
		ccr = 1.6
		debt = price * (col / ccr)

		# check buck balance
		self.assertEqual(debt, balance(buck, user1))

		# check all cdp values
		cdp = table(buck, "cdp")
		self.assertEqual(100, amount(cdp["collateral"]))
		self.assertEqual(debt, amount(cdp["debt"]))
		self.assertEqual(100000, amount(cdp["rex"]))
		self.assertAlmostEqual(0, float(cdp["acr"]))
		self.assertEqual("user1", cdp["account"])

		# check open multiple cdps

# main

if __name__ == "__main__":
	unittest.main()