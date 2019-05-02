# coding: utf8
# Copyright © Scruge 2019.
# This file is part of ScrugeX.
# Created by Yaroslav Erohin.

import unittest
from eosfactory.eosf import *
import eosfactory.core.setup as setup 
from methods import *
import test
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
		transfer(eosio_token, master, user1, "1000000000000.0000 EOS", "")

	def run(self, result=None):
		super().run(result)

	# tests

	def test(self):

		##################################
		COMMENT("Initialize")

		try:
			transfer(eosio_token, buck, user1, "1000000000.0000 EOS", "")
		except: pass

		destroy(buck)
		maketime(buck, 0)
		update(buck, 100)

		transfer(eosio_token, user1, buck, "1000000000.0000 EOS", "deposit")

		# mature rex
		test.init(2)
		maketime(buck, test.get_time())
		update(buck, test.get_price())

		##################################
		COMMENT("Open CDP")

		cdp_table = test.table
		for cdp in sorted(cdp_table, key=lambda x:int(x.id)):
			# print(cdp)
			ccr = 0 if cdp.cd > 999999 else cdp.cd
			open(buck, user1, ccr, cdp.acr, asset(cdp.collateral, "REX"))

		self.compare(buck, cdp_table)

		# ##################################
		# COMMENT("Liquidation sorting")
		
		# top_debtors = get_debtors(buck, limit=20)
		# for i in range(0, len(top_debtors)):
		# 	debtor = top_debtors[i]
		# 	if amount(debtor["debt"]) == 0: break # unsorted end of the table
		# 	self.match(cdp_table[i * -1 - 1], debtor)

		# top_liquidators = get_liquidators(buck, limit=20)
		# for i in range(0, len(top_liquidators)):
		# 	liquidator = top_liquidators[i]
		# 	if liquidator["acr"] == 0: break # unsorted end of the table
		# 	self.match(cdp_table[i], liquidator)


		##################################
		COMMENT("Start rounds")

		# rounds
		for i in range(0, 20):
			COMMENT(f"Round {i+1}")

			# actions
			result = test.run_round()
			round_time = result[0]
			actions = result[1]

			for action in actions:
				print(action)

				if action[0][0] == "reparam":
					cdp = action[0][1]
					col = asset(action[0][2], "REX")
					debt = asset(action[0][3], "BUCK")

					get_cdp(buck, cdp)

					if action[1] == False:
						assertRaises(self, lambda: reparam(buck, user1, cdp, debt, col))
					else: reparam(buck, user1, cdp, debt, col)

				elif action[0][0] == "acr":
					cdp = action[0][1]
					acr = action[0][2]

					# get_cdp(buck, cdp)

					if action[1] == False:
						assertRaises(self, lambda: changeacr(buck, user1, cdp, acr))
					else: changeacr(buck, user1, cdp, acr)

				elif action[0][0] == "redeem":
					quantity = asset(action[0][1], "BUCK")

					if action[1] == False:
						assertRaises(self, lambda: redeem(buck, user1, quantity))
					else: redeem(buck, user1, quantity)

			maketime(buck, round_time)
			update(buck, test.get_price())
			run(buck)
			run(buck)
			run(buck)

			table(buck, "cdp")

			self.compare(buck, cdp_table)



	def match(self, cdp, row):
		print(cdp)
		self.assertEqual(cdp.acr, row["acr"], "ACRs don't match")
		self.assertAlmostEqual(unpack(cdp.debt), amount(row["debt"]), 3, "debts don't match")
		self.assertAlmostEqual(unpack(cdp.collateral), amount(row["collateral"]), 3, "collaterals don't match")
		self.assertEqual(cdp.time, row["modified_round"], "rounds modified don't match")
		print(f"+ Matched cdp #{cdp.id}")

	def compare(self, buck, cdp_table):
		for cdp in cdp_table:
				row = get_cdp(buck, cdp.id)
				if cdp.time == 0: cdp.time = int(row["modified_round"]) # update round time
				self.match(cdp, row)

# main

if __name__ == "__main__":
	unittest.main()
