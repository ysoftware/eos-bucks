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
		price = 100
		destroy(buck)
		update(buck, price)
		transfer(eosio_token, user1, buck, "1000000000000.0000 EOS", "deposit")

		# generate cdps
		cdp_table = test.gen(2, 5, price)
		
		for cdp in sorted(cdp_table, key=lambda x:int(x.id)):
			ccr = 0 if cdp.cd > 999999 else cdp.cd
			open(buck, user1, ccr, cdp.acr, asset(cdp.collateral, "EOS"))

		sleep(1)
		update(buck, price)
		run(buck)

		sleep(1)
		update(buck, price)
		run(buck)	

		for cdp in cdp_table:
			print(cdp)
			row = get_cdp(buck, cdp.id)
			cdp.new_time(int(row["modified_round"]))
			self.assertEqual(cdp.acr, row["acr"], "open: acr does not match")
			self.assertEqual(unpack(cdp.debt), amount(row["debt"]), "open: debt does not match")
			self.assertEqual(unpack(cdp.collateral), amount(row["collateral"]), "open: collateral does not match")




# main

if __name__ == "__main__":
	unittest.main()
