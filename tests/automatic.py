# coding: utf8
# Copyright © Scruge 2019.
# This file is part of ScrugeX.
# Created by Yaroslav Erohin.

import unittest
from eosfactory.eosf import *
from methods import *
from time import sleep
import test
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
		transfer(eosio_token, master, user1, "1000000000000.0000 EOS", "")

	def run(self, result=None):
		super().run(result)

	# tests

	def test(self):

		# initialize
		price = 1000
		destroy(buck)
		update(buck, price)

		transfer(eosio_token, user1, buck, "1000000000000.0000 EOS", "deposit")

		COMMENT("Open CDP")
		cdp_table = test.gen(10, 20, price)
		for cdp in sorted(cdp_table, key=lambda x:int(x.id)):
			ccr = 0 if cdp.cd > 999999 else cdp.cd
			open(buck, user1, ccr, cdp.acr, asset(cdp.collateral, "EOS"))

		sleep(2) # wait for maturity

		update(buck, price)
		run(buck)	

		self.compare(buck, cdp_table)

		COMMENT("Liquidation")
		price = int(price * 0.8)

		test.liquidation(cdp_table, price)
		update(buck, price)
		run(buck)
		self.compare(buck, cdp_table)


	def compare(self, buck, cdp_table):
		for cdp in cdp_table:
				print(cdp)
				row = get_cdp(buck, cdp.id)
				if cdp.time == 0:
					cdp.time = int(row["modified_round"])
				self.assertEqual(cdp.acr, row["acr"], "open: acr does not match")
				self.assertEqual(unpack(cdp.debt), amount(row["debt"]), "open: debt does not match")
				self.assertEqual(unpack(cdp.collateral), amount(row["collateral"]), "open: collateral does not match")
				self.assertEqual(unpack(cdp.time), amount(row["modified_round"]), "open: rounds modified does not match")



# main

if __name__ == "__main__":
	unittest.main()
