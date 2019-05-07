# coding: utf8
# Copyright © Scruge 2019.
# This file is part of ScrugeX.
# Created by Yaroslav Erohin.

import unittest, random, string
from eosfactory.eosf import *
import eosfactory.core.setup as setup 
from methods import *
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

	def test(self):

		# while True:

			##################################
			COMMENT("Initialize")

			try:
				transfer(eosio_token, buck, user1, "1000000000.0000 EOS", "")
			except: pass

			destroy(buck)
			maketime(buck, 0)
			update(buck, 1)

			transfer(eosio_token, user1, buck, "1000000000.0000 EOS", "deposit")

			# mature rex
			test.init()
			maketime(buck, test.time)
			update(buck, test.price)

			##################################
			COMMENT("Open CDP")

			for cdp in sorted(test.table, key=lambda x:int(x.id)):
				ccr = int(0 if cdp.cd > 999999 else cdp.cd)
				open(buck, user1, ccr, cdp.acr, asset(cdp.collateral, "REX"))

			##################################
			COMMENT("Initial matching")
			
			self.compare(buck, test.table)

			##################################
			COMMENT("Start rounds")

			for round_i in range(0, random.randint(30, 70)):
				print("\n\n\n\n")
				COMMENT(f"Round {round_i+1}")

				##################################
				COMMENT(f"Actions for round {round_i+1}")
				
				# actions
				result = test.run_round(balance(buck, user1) * 10000)
				round_time = result[0]
				actions = result[1]

				if len(actions) == 0:
					COMMENT("No actions were performed")
				else:
					for action in actions:
						if action[0][0] == "reparam":
							cdp = action[0][1]
							col = asset(action[0][2], "REX")
							debt = asset(action[0][3], "BUCK")

							print("reparam", col, debt)
							if action[1] == False:
								assertRaises(self, lambda: reparam(buck, user1, cdp, debt, col))
							else: reparam(buck, user1, cdp, debt, col)

						elif action[0][0] == "acr":
							cdp = action[0][1]
							acr = action[0][2]

							if action[1] == False:
								assertRaises(self, lambda: changeacr(buck, user1, cdp, acr))
							else: changeacr(buck, user1, cdp, acr)

						elif action[0][0] == "redeem":

							quantity = asset(action[0][1], "BUCK")

							if action[1] == False:
								balance(buck, user1)
								assertRaises(self, lambda: redeem(buck, user1, quantity))
							else: redeem(buck, user1, quantity)

				test.print_table()

				maketime(buck, round_time)
				update(buck, test.price)
				
				##################################
				COMMENT(f"Matching after round {round_i+1}")

				# match taxes
				taxation = table(buck, "taxation")

				print("idp", test.IDP, "cit", test.CIT, "aec", test.AEC, "tec", test.TEC)
				self.assertAlmostEqual(unpack(test.IDP), unpack(taxation["insurance_pool"]), 4, "insurance pools don't match")
				self.assertAlmostEqual(unpack(test.AEC), unpack(taxation["r_aggregated"]), 0, "aggregated excesses don't match")
				self.assertAlmostEqual(unpack(test.TEC), unpack(taxation["r_total"]), 0, "total excesses don't match")
				self.assertAlmostEqual(test.CIT, taxation["r_collected"], 0, "collected insurances don't match")
				print("+ Matched insurance pools")

				# match cdps

				print("\n")
				test.print_table()
				self.compare(buck, test.table)

				# match supply

				COMMENT(f"Round {round_i+1} complete")



	def compare(self, buck, cdp_table):

		print("debtors")
		top_debtors = get_debtors(buck, limit=5)
		for i in range(0, len(top_debtors)):
			debtor = top_debtors[i]
			if amount(debtor["debt"]) == 0: break # unsorted end of the table
			self.match(test.table[i * -1 - 1], debtor)

		print("liquidators")
		top_liquidators = get_liquidators(buck, limit=5)
		for i in range(0, len(top_liquidators)):
			liquidator = top_liquidators[i]
			if amount(liquidator["debt"]) > 0: break # unsorted end of the table
			self.match(test.table[i], liquidator)



	def match(self, cdp, row):
		print(cdp)
		print("#" + str(row["id"]), row["collateral"], row["debt"], row["acr"], row["modified_round"])

		self.assertEqual(cdp.id, row["id"], "attempt to match different CDPs")
		self.assertEqual(cdp.acr, row["acr"], "ACRs don't match")		
		self.assertAlmostEqual(unpack(cdp.debt), amount(row["debt"]), 4, "debts don't match")
		self.assertAlmostEqual(unpack(cdp.collateral), amount(row["collateral"]), 4, "collaterals don't match")
		self.assertEqual(cdp.time, row["modified_round"], "rounds modified don't match")
		print(f"+ Matched cdp #{cdp.id}")


# main

if __name__ == "__main__":
	unittest.main()
