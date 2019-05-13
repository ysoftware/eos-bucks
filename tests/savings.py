# coding: utf8
# Copyright © Scruge 2019.
# This file is part of ScrugeX.
# Created by Yaroslav Erohin.

import unittest
from eosfactory.eosf import *
import eosfactory.core.setup as setup 
from methods import *
from time import sleep
import string

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
		create_account("user2", master, "user2")
		transfer(eosio_token, master, user1, "1000000000.0000 EOS", "")

	def run(self, result=None):
		super().run(result)

	# tests 
	
	def test(self):

		time = 0
		maketime(buck, time)
		update(buck, 1)

		transfer(eosio_token, user1, buck, "1000000000.0000 EOS", "deposit")

		# mature rex
		time += 3_000_000
		maketime(buck, time)

		open(buck, user1, 200, 0, "100000000.0000 REX")
		open(buck, user1, 200, 0, "100000000.0000 REX")
		open(buck, user1, 200, 0, "100000000.0000 REX")

		# save
		maketime(buck, time)
		update(buck)

		save(buck, user1, "100.0000 BUCK")

		table(buck, "taxation")

		# save more
		time += 1_000
		maketime(buck, time)
		update(buck)

		save(buck, user1, "100000.0000 BUCK")

		tax = table(buck, "taxation")

		# take savings
		balance(buck, user1)
		take(buck, user1, 1000000)
		take(buck, user1, "100000.0000 BUCK")

		# oracle
		time += 1_000
		maketime(buck, time)
		update(buck)

		balance(buck, user1)
		tax = table(buck, "taxation")

		self.assertAlmostEqual(0, amount(tax["savings_pool"]), -1, "Savings pool is not empty")

		table(buck, "accounts", user1)



		


# main

if __name__ == "__main__":
	unittest.main()