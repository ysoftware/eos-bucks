# coding: utf8
# Copyright © Scruge 2019.
# This file is part of ScrugeX.
# Created by Yaroslav Erohin.

import unittest
from eosfactory.eosf import *
from methods import *
import eosfactory.core.setup as setup 
from time import sleep
import string
import datetime
import test

verbosity([Verbosity.INFO, Verbosity.OUT, Verbosity.TRACE, Verbosity.DEBUG])

CONTRACT_WORKSPACE = "eos-buck"

# methods

class Test(unittest.TestCase):

	# setup
	setup.is_raise_error = True

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

		transfer(eosio_token, master, user1, "1000000000000.0000 EOS", "")

	def run(self, result=None):
		super().run(result)

	# tests

	### check rex values when selling rex

	def test(self):
		MATURE = 435000
		SCENARIO("Test cdp reparametrization")

		time = 0
		maketime(buck, time)
		update(buck, 100)
		test.update(100, time)

		transfer(eosio_token, user1, buck, "1000000000.0000 EOS", "deposit")

		# mature rex
		time += MATURE
		maketime(buck, time)
		update(buck, 100)
		test.update(100, time)

		open(buck, user1, 200, 0, "100000.0000 REX")

		cdp = test.CDP(100000*10**4, 50000*10**4, 200, 0, 0, test.get_time()) # collateral, debt, cd, acr, id, time
		test.table = [cdp]

		# give debt (maturity)
		time += 10
		maketime(buck, time)
		update(buck, 100)
		test.update(100, time)

		# match opened cdp
		self.match(cdp, get_cdp(buck, 0))

		# create request
		reparam(buck, user1, 0, "0.0000 BUCK", "-10.0000 REX")

		time += 2629900
		maketime(buck, time)
		update(buck, 100)
		test.update(100, time)

		# run reparam
		test.reparametrize(0, -10*10**4, 0, 100, 100) # c, d, price, old_price

		self.match(cdp, get_cdp(buck, 0))


	def match(self, cdp, row):
		# print(cdp)
		self.assertEqual(cdp.acr, row["acr"], "ACRs don't match")
		self.assertAlmostEqual(unpack(cdp.debt), amount(row["debt"]), 3, "debts don't match")
		self.assertAlmostEqual(unpack(cdp.collateral), amount(row["collateral"]), 3, "collaterals don't match")
		# self.assertEqual(cdp.time, row["modified_round"], "rounds modified don't match")
		print(f"+ Matched cdp #{cdp.id}")



# main

if __name__ == "__main__":
	unittest.main()