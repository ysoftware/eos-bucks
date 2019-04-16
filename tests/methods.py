# coding: utf8
# Copyright © Scruge 2019.
# This file is part of ScrugeX.
# Created by Yaroslav Erohin.

import unittest
from eosfactory.eosf import *
from datetime import datetime
import time

def timeMs():
	return int(time.time()*1000.0)

# actions

def deploy(contract):
	if not contract.is_built():
		contract.build()
	contract.deploy()

def perm(contract, key):
	contract.set_account_permission(
		Permission.ACTIVE,
		{
				"threshold" : 1,
				"keys" : [{ "key": key.key_public, "weight": 1 }],
				"accounts": [{
					"permission": {
						"actor": contract,
						"permission": "eosio.code"
					},
					"weight": 1
				}],
			},
		Permission.OWNER, (contract, Permission.OWNER))

def create_issue(contract, to, symbol):
	contract.push_action("create",
		{
			"issuer": to,
			"maximum_supply": "1000000000.0000 {}".format(symbol)
		},
		permission=[(contract, Permission.ACTIVE)])
	contract.push_action("issue",
		{
			"to": to,
			"quantity": "1000000000.0000 {}".format(symbol),
			"memo": ""
		},
		permission=[(to, Permission.ACTIVE)])

def destroy(contract):
	contract.push_action("zdestroy", "[]", permission=[(contract, Permission.ACTIVE)])

def transfer(contract, fromAccount, to, quantity, memo=""):
	contract.push_action("transfer",
		{
			"from": fromAccount,
			"to": to,
			"quantity": quantity,
			"memo": memo
		},
		permission=[(fromAccount, Permission.ACTIVE)])

def buyram(contract):
	contract.push_action("buyram", "[]", permission=[(contract, Permission.ACTIVE)])

# contract actions

def open(contract, user, ccr, acr, quantity, token_contract):
	transfer(token_contract, user, contract, quantity, "deposit")
	contract.push_action("open",
		{
			"account": user,
			"ccr": ccr,
			"acr": acr,
			"quantity": quantity
		},
		permission=[(user, Permission.ACTIVE)])

def update(contract, eos=2):
	contract.push_action("update",
		{ "eos_price": eos },
		permission=[(contract, Permission.ACTIVE)])

def close(contract, user, cdp_id):
	contract.push_action("closecdp",
		{
			"cdp_id": cdp_id
		},
		permission=[(user, Permission.ACTIVE)])

def run(contract, max=15):
	contract.push_action("run",
		{
			"max": max
		},
		permission=[(contract, Permission.ACTIVE)])

def reparam(contract, user, cdp_id, change_debt, change_collat):
	contract.push_action("change",
		{
			"cdp_id": cdp_id,
			"change_debt": change_debt,
			"change_collateral": change_collat
		},
		permission=[(user, Permission.ACTIVE)])

def changeacr(contract, user, cdp_id, acr):
	contract.push_action("changeacr",
		{
			"cdp_id": cdp_id,
			"acr": acr
		},
		permission=[(user, Permission.ACTIVE)])

def redeem(contract, user, quantity):
	contract.push_action("redeem",
		{
			"account": user,
			"quantity": quantity
		},
		permission=[(user, Permission.ACTIVE)])

def withdraw(contract, user, quantity):
	contract.push_action("redeem",
		{
			"from": user,
			"quantity": quantity
		},
		permission=[(user, Permission.ACTIVE)])

# requests

def balance(token, account, unwrap=True):
	rows = token.table("accounts", account).json["rows"]
	if len(rows) == 0:
		return 0 if unwrap else "0 -"
	balance = rows[0]["balance"]
	if unwrap:
		return amount(balance, force=False)
	else:
		return balance

def amount(quantity, force=True, default=0):
	split = quantity.split(" ")
	if isinstance(split, list):
		if len(split) == 2: return float(split[0])
		elif force: assert("value is not an asset")
		else: return default
	assert("value is not an asset")

def table(contract, table, scope=None, row=0, element=None):
	if scope == None: scope = contract
	data = contract.table(table, scope).json["rows"]
	if len(data) > 0: data = data[row] 

	if element != None: return data[element]
	else: return data

# assert

def assertErrors(self, tests):
	for test in tests:
		if isinstance(test, list) and len(test) == 2:
			assertRaisesMessage(self, test[0], test[1])
		else:
			assertRaises(self, test)

def assertRaisesMessage(self, message, func):
	with self.assertRaises(Error) as c:
		func()
	self.assertIn(message.lower(), c.exception.message.lower())
	print("+ Exception raised: \"%s\"" % message)

def assertRaises(self, func):
	with self.assertRaises(Error):
		func()
	print("+ Exception raised")