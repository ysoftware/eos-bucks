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
		create_account("scrugescruge", master, "scrugescruge")
		
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
		transfer(eosio_token, master, user1, "150.0000 EOS", "")

	def run(self, result=None):
		super().run(result)

	# tests

	def test(self):
		SCENARIO("Test close cdp")
		update(buck)

		# rex gives dividents, mock contract doesn't.
		# we need to have spare eos on contract to ensure proper workflow
		# when taking out collateral
		transfer(eosio_token, master, buck, "100.0000 EOS", "rex fund") 

		open(buck, user1, 1.5, 0, "100.0000 EOS", eosio_token) # 0

		sleep(2)
		update(buck)

		open(buck, user1, 1.6, 0, "50.0000 EOS", eosio_token) # 1 request to get more bucks for closing

		table(buck, "maturityreq")
		table(buck, "cdp")

		sleep(2)
		update(buck)

		sleep(2)
		update(buck)

		taxpool = amount(table(buck, "stat1", element="tax_pool"))

		close(buck, user1, 0)

		# check close request
		self.assertEqual(0, table(buck, "closereq", element="cdp_id"))

		run(buck)

		# check close request still there
		self.assertEqual(0, table(buck, "closereq", element="cdp_id"))

		sleep(2)
		update(buck)

		taxpool_left = amount(table(buck, "stat1", element="tax_pool"))

		# check close request gone
		self.assertEqual(0, len(table(buck, "closereq")))

		# check first cdp id (should be 1)
		self.assertEqual(1, table(buck, "cdp", element="id"))

		# check collateral return; rex.eos = 998
		self.assertEqual(101.2024, fundbalance(buck, user1))

		# check debt burned (left over from cdp #1)
		# to-do find fund by userid
		return_amount = 58.5834 + taxpool - taxpool_left

		self.assertEqual(return_amount, balance(buck, user1))



# main

if __name__ == "__main__":
	unittest.main()