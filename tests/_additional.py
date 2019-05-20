# coding: utf8
# Copyright © Scruge 2019.
# This file is part of Buck Protocol.
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
		SCENARIO("Test liquidation and Redemption")
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
		transfer(eosio_token, master, user1, "100000000.0000 EOS", "")
		
		create_account("user2", master, "user2")
		transfer(eosio_token, master, user1, "100000000.0000 EOS", "")
		
		create_account("user3", master, "user3")
		transfer(eosio_token, master, user1, "100000000.0000 EOS", "")
		
		create_account("user4", master, "user4")
		transfer(eosio_token, master, user1, "100000000.0000 EOS", "")
		
		create_account("user5", master, "user5")
		transfer(eosio_token, master, user1, "1000000000.0000 EOS", "")
		
		create_account("user6", master, "user6")
		transfer(eosio_token, master, user1, "1000000000.0000 EOS", "")
		
		create_account("user7", master, "user7")
		transfer(eosio_token, master, user1, "1000000000.0000 EOS", "")
		
		create_account("user8", master, "user8")
		transfer(eosio_token, master, user1, "1000000000.0000 EOS", "")
		
		create_account("user9", master, "user9")
		transfer(eosio_token, master, user1, "1000000000.0000 EOS", "")
	
		create_account("user10", master, "user10")
		transfer(eosio_token, master, user1, "1000000000.0000 EOS", "")

	def run(self, result=None):
		super().run(result)

	# tests

	def test(self):
	
	# I need cdp(cdp_id, debt/collateral/acr) function
	
	LF = 0.1 # Liquidation Fee
	RF = 0.02 # Redemption Fee
	
		def x(c,d,price, LF):
			return (c*price*(1.5-0.5*LF)+d*(0.75*LF-1.25))/(2.5-1.5*LF)
	
	# calculate how much debt liquidator receives
		def liq(liq_col, d_col, debt, acr, p):
			ccr = d_col * p / debt
			if ccr >= 1 + LF:
				L = LF
			else: 
				L = ccr - 1
			x = x(d_col,debt,p,L)
			val = min((1.5-ccr)*debt+x,(liq_col*p*(1-LF))/(acr*(1-LF)-1))
			return val
				
	
		price = 2.0
	
		update(buck, price)
		
		# Round 1: Initialise CDPs
		
		rand = 
		

		open(buck, user1, 1.6, 0) 
		transfer(eosio_token, user1, buck, rand, "")
		debt1 = price * (rand/1.6)
		
		open(buck, user2, 2.1, 0) 
		transfer(eosio_token, user2, buck, rand, "")
		debt2 = price * (rand/2.1)
		
		open(buck, user3, 2.6, 0) 
		transfer(eosio_token, user3, buck, rand, "")
		debt3 = price * (rand/2.8)
		
		open(buck, user4, 0, 2.0) 
		transfer(eosio_token, user4, buck, liq(rand * 10, rand, rand * price  / 1.6, 1.5, price / 2)*price*8 , "")
		
		open(buck, user5, 0, 4.0) 
		transfer(eosio_token, user5, buck, liq(rand * 10, rand, rand * price  / 1.6, 1.5, price / 2)*price*16, "")
		
		open(buck, user6, 3.1, 2.0) 
		transfer(eosio_token, user6, buck, rand, "")
		
		open(buck, user7, 5.0, 2.5) 
		transfer(eosio_token, user7, buck, rand, "")
		
		open(buck, user8, 10.0, 2.5) 
		transfer(eosio_token, user8, buck, rand, "")
		
		
		
		
		# Round 2: Crash the price and test what happens when all debt can't be bailed out
		
		price = price / 2
	
		
		update(buck, price)
		
		
		#Correctness of liquidation
		
		d1 = cdp(user1,debt)
		
		
		#CDPs 1,4,5
		assertAlmostEqual(
	
		
		
		
		# Check that redemption
		
		
		 
		 
	
		
	

# main

if __name__ == "__main__":
	unittest.main()