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

def perm(eosioscrugex, key):
	eosioscrugex.set_account_permission(
		Permission.ACTIVE,
		{
				"threshold" : 1,
				"keys" : [{ "key": key.key_public, "weight": 1 }],
				"accounts": [{
					"permission": {
						"actor": "eosioscrugex",
						"permission": "eosio.code"
					},
					"weight": 1
				}],
			},
		Permission.OWNER, (eosioscrugex, Permission.OWNER))

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

def transfer(contract, fromAccount, to, quantity, memo):
	contract.push_action("transfer",
		{
			"from": fromAccount,
			"to": to,
			"quantity": quantity,
			"memo": memo
		},
		permission=[(fromAccount, Permission.ACTIVE)])

# contract actions

def open(contract, user, ccr, acr):
	contract.push_action("open",
		{
			"account": user,
			"ccr": ccr,
			"acr": acr
		},
		permission=[(user, Permission.ACTIVE)])


def buyram(eosioscrugex):
	eosioscrugex.push_action("buyram", "[]", permission=[(eosioscrugex, Permission.ACTIVE)])

# requests

def balance(token, account):
	return amount(token.table("accounts", account).json["rows"][0]["balance"])

def amount(quantity):
	return float(quantity.split(" ")[0])

def table(contract, table, scope=None, row=0, element=None):
	if scope == None:
		scope = contract
	data = contract.table(table, scope).json["rows"][row]
	if element != None:
		return data[element]
	else:
		return data

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