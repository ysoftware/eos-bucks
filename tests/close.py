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
		SCENARIO("Test close cdp")
		reset()

		create_master_account("master")

		create_account("eosio_token", master, "eosio.token")

		key = CreateKey(is_verbose=False)
		create_account("buck", master, "buck", key)
		perm(buck, key)

		deploy(Contract(eosio_token, "02_eosio_token"))
		deploy(Contract(buck, "eos-bucks/src"))

		# Distribute tokens

		create_issue(eosio_token, master, "EOS")

		# Users

		create_account("user1", master, "user1")
		transfer(eosio_token, master, user1, "1000000.0000 EOS", "")

	def run(self, result=None):
		super().run(result)

	# tests

	def test(self):
		init(buck)

		update(buck, 3.6545)

		open(buck, user1, 1.5, 0) # 0
		transfer(eosio_token, user1, buck, "100.0000 EOS", "")

		open(buck, user1, 1.5, 0) # 1
		transfer(eosio_token, user1, buck, "100.0000 EOS", "")

		table(buck, "cdp")

		close(buck, user1, 0)
		run(buck)

		update(buck, 3.7)
		run(buck)
		sleep(2)

		close(buck, user1, 1)

		table(buck, "cdp")

		update(buck, 3.71)
		run(buck)
		sleep(2)

		table(buck, "cdp")


# main

if __name__ == "__main__":
	unittest.main()