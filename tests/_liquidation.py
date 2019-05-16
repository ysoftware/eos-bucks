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
		SCENARIO("Test liquidation")
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
		transfer(eosio_token, master, user1, "1000000.0000 EOS", "")

	def run(self, result=None):
		super().run(result)

	# tests

	def test(self):
		update(buck, 3.6545)

		open(buck, user1, 1.5, 0) # 0
		transfer(eosio_token, user1, buck, "100.0000 EOS", "")

		update(buck, 3.6545)

		open(buck, user1, 1.7, 2.0) # 1
		transfer(eosio_token, user1, buck, "100.0001 EOS", "")

		update(buck, 3.6545)

		open(buck, user1, 1.6, 1.5) # 2
		transfer(eosio_token, user1, buck, "100.0002 EOS", "")

		update(buck, 3.6545)

		open(buck, user1, 3.0, 1.5) # 3
		transfer(eosio_token, user1, buck, "100.0003 EOS", "")

		update(buck, 3.6545)

		open(buck, user1, 3.0, 0) # 4
		transfer(eosio_token, user1, buck, "100.0004 EOS", "")

		table(buck, "cdp")
		
		update(buck, 3.4)
		
		update(buck, 3.8)

		update(buck, 3.5)
		
		table(buck, "cdp")

# main

if __name__ == "__main__":
	unittest.main()