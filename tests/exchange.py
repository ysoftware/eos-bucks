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
		create_account("user3", master, "user3")
		create_account("user4", master, "user4")

		transfer(eosio_token, master, user1, "1000000.0000 EOS", "")
		transfer(eosio_token, master, user2, "1000000.0000 EOS", "")
		transfer(eosio_token, master, user3, "1000000.0000 EOS", "")

	def run(self, result=None):
		super().run(result)

	# tests 
	
	def test(self):

		maketime(buck, 0)
		update(buck)

		transfer(eosio_token, user1, buck, "1000000.0000 EOS", "deposit")
		transfer(eosio_token, user2, buck, "1000000.0000 EOS", "exchange")
		transfer(eosio_token, user3, buck, "1000000.0000 EOS", "exchange")

		maketime(buck, 3_000_000)
		open(buck, user1, 200, 0, "100000.0000 REX")

		transfer(buck, user1, user4, "5000.0000 BUCK", "")

		table(buck, "stat")

		###################################
		COMMENT("Exchange")

		maketime(buck, 3_000_001)
		exchange(buck, user2, "15.0000 EOS")

		maketime(buck, 3_000_002)
		exchange(buck, user3, "5.0000 EOS")

		maketime(buck, 3_000_003)
		exchange(buck, user1, "1000.0000 BUCK")

		maketime(buck, 3_000_004)
		exchange(buck, user4, "3000.0000 BUCK")

		#################################
		COMMENT("Check")

		table(buck, "exchange")

		#################################
		COMMENT("Complete")
		
		maketime(buck, 4_000_000)
		update(buck)

		table(buck, "exchange")




# main

if __name__ == "__main__":
	unittest.main()