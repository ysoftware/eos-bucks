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
		transfer(eosio_token, master, user1, "1000000000.0000 EOS", "")

	def run(self, result=None):
		super().run(result)

	# tests 
	
	def test(self):
		update(buck)

		open(buck, user1, 2, 0, "100000.0000 EOS", eosio_token)
		open(buck, user1, 2, 0, "100010.0000 EOS", eosio_token)
		open(buck, user1, 2, 0, "100020.0000 EOS", eosio_token)

		# oracle
		sleep(2)
		update(buck)

		# check aggregated
		save(buck, user1, "300000.000 BUCK")
		# redeem(buck, user1, "300000.000 BUCK")

		# oracle
		sleep(10)
		update(buck)

		table(buck, "taxation")

		# take savings
		balance(buck, user1)
		take(buck, user1, "300000.000 BUCK")

		table(buck, "taxation")

		# oracle
		sleep(2)
		update(buck)

		balance(buck, user1)
		table(buck, "taxation")



		


# main

if __name__ == "__main__":
	unittest.main()