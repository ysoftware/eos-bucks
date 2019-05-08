import random
from math import exp

table = []
CR = 150 # collateral ratio
LF = 10 # Liquidation Fee
IR = 20 # insurance ratio
SR = 100 - IR # savings ratio
r = 0.05 # interest rate
IDP = 0 # insurance dividend pool
TEC = 0 # total excess collateral
AEC = 0 # aggregated excess collateral
CIT = 0 # collected insurance tax
commission = 20 # our commission
time = 0 # initial time
oracle_time = 0

def time_now():
	global time
	time += 1_000_000 # random.randint(1000, 10000) * 1000 # maturity time up to 3 months
	return time

def epsilon(value): return value / 500

class CDP:
	def __init__(self, collateral, debt, cd, acr, id, time):
		self.collateral = int(collateral)
		self.debt = int(debt)
		self.cd = cd
		self.acr = acr
		self.id = id
		self.time = time
	def __repr__(self):
		string = "c: " + str(int(self.collateral // 10000)) + "."  + str(int(self.collateral % 10000))
		string2 = "d: " + ("0\t" if self.debt == 0 else (str(int(self.debt // 10000)) + "." + str(int(self.debt % 10000))))
		return "#" + str(self.id)  + "\t" + string + "\t" + string2  + "\t" + "acr: " + str(self.acr) + "\tcd: " + str(self.cd) + "\t" + "\tacr:" + str(self.acr) + "\ttime: " + str(self.time//1_000_000) + "M"

	def add_debt(self,new_debt):
		self.debt = self.debt + int(new_debt)
	def add_collateral(self, new_collateral):
		self.collateral = int(self.collateral) + int(new_collateral)
	def new_cd(self, cd_new):
		self.cd = cd_new
	def new_acr(self,acr_new):
		self.acr = int(acr_new)
	def new_debt(self, debt_new):
		self.debt = int(debt_new)
	def new_collateral(self, collateral_new):
		self.collateral = int(collateral_new)
	def new_time(self, time_new):
		self.time = time_new		
	
# Functions for generation of sorted CDPs with random values

def generate_liquidators(k):
	global TEC, time
	rand = random.randrange(1000000,10000000,10000)
	rand2 = random.randint(150,200)
	liquidator = CDP(rand, 0, 9999999, rand2, 0, time)
	TEC += liquidator.collateral * 100 // liquidator.acr
	liquidators = [liquidator]
	for i in range (0,k):
		helper = liquidators[i].acr
		rand2 = random.randint(helper+1,helper+2)
		liquidators.append(CDP(rand, 0, 9999999, rand2,i+1, time))
		liquidator = liquidators[len(liquidators)-1]
		TEC += liquidator.collateral * 100 // liquidator.acr
	return liquidators

def generate_debtors(k, n):
	global time, price
	rand = random.randrange(1000000,10000000,10000) # collateral
	rand2 = random.randint(150,155) # cd
	ccr = rand2
	debtor = CDP(rand, 0, rand2, 0, k+1, time)
	debtor.add_debt(debtor.collateral * price // debtor.cd)
	debtor.new_cd(debtor.collateral * price / debtor.debt)
	debtors = [debtor]
	for i in range (k+1,n):
		rand = random.randrange(1000000,10000000,10000)
		helper = ccr

		acr = random.randint(100, 160)
		if acr < 150: acr = 0

		rand2 = random.randint(helper+1, helper + 2)
		ccr = rand2
		debtor = CDP(rand, 0, rand2, acr, i+1, time)
		debtor.add_debt(debtor.collateral * price // debtor.cd)	
		debtor.new_cd(debtor.collateral * price / debtor.debt)
		debtors.insert(0, debtor)
	return debtors

def gen(k, n):
	global table, time, price
	liquidators = generate_liquidators(k)
	debtors = generate_debtors(k, n)
	table = liquidators + debtors

# Function for inserting CDP into the table
	
def cdp_insert(cdp):
	global table

	if table == []:
		table = [cdp]
		return
	c = cdp.collateral
	d = cdp.debt
	acr = cdp.acr
	cd = cdp.cd
	len_table = len(table)
	if (d <= epsilon(d)) and acr == 0:
		return 
	elif (d <= epsilon(d)) and acr != 0:
		for i in range(0, len_table):
			cdp2 = table[i]
			c2 = cdp2.collateral
			d2 = cdp2.debt
			if d2 > epsilon(d2):
				table.insert(i, cdp)
				return
			acr2 = cdp2.acr
			cd2 = cdp2.cd
			if acr2 == 0:
				table.insert(i, cdp)
				return
			else:
				if c * 10_000_000 // acr > c2 * 10_000_000 // acr2:
					table.insert(i, cdp)
					return
		table.append(cdp)
		return
	else:
		# debtors
		for i in range(len(table)-1, -1, -1):
			cdp2 = table[i]
			d2 = cdp2.debt
			if d2 <= epsilon(d2): # debt 0
				table.insert(i+1, cdp)
				return 
			c2 = cdp2.collateral
			d2 = cdp2.debt

			if c * 10_000_000 // d  <  c2 * 10_000_000 // d2:
				table.insert(i+1, cdp)
				return 
		table.insert(0, cdp)
		return 
	
# Function for pulling out CDP from the table by querying its ID
def cdp_index(id):
	global table
	if len(table) == 0:
		return False
	for i in range(0, len(table)):
		if table[i].id == id:
			return i
	return False

def print_table():
	global table
	if table == []:
		print("table is empty")
	else:
		print("\nfull table:")
		for i in range(0,len(table)):
			print(table[i])	
			
# Helper functions for calculations

def calc_ccr(cdp, price):
	ccr = cdp.cd * price // 100
	return int(ccr)
	
def calc_lf(cdp, price, cr, lf):
	ccr = calc_ccr(cdp, price)
	if ccr >= 100 + lf:
		l = lf
	elif ccr < 75:
		l = -25
	else: 
		l = ccr - 100
	return l
	
def x_value(d, l, c, p):
		x = int((100+l)*(750*d-5*c*p) // (50000-1500*l))
		return x
		
def calc_bad_debt(cdp, price, cr, lf):
	ccr = calc_ccr(cdp, price)
	val = (cr-ccr) * cdp.debt // 100 + x_value(cdp.debt, lf, cdp.collateral, price)
	return int(val)
	
def calc_val(cdp, liquidator, price, cr, lf):
	l = calc_lf(cdp, price, cr, lf)
	c = liquidator.collateral
	d = liquidator.debt
	acr = int(liquidator.acr)
	bad_debt = calc_bad_debt(cdp, price, cr, l)
	bailable = ((c*price)-(d*acr)) * (100-l) // (acr*(100-l)-10_000)
	print("bad_debt", bad_debt)
	print("bailable", bailable)
	return min(bad_debt, bailable, cdp.debt)

# Taxes

def add_tax(cdp, price):
	global IDP, CIT, TEC, oracle_time

	if cdp.debt > epsilon(cdp.debt) and oracle_time > cdp.time:
		print("add tax", cdp.id)
		dm = 1000000000000
		v = int((exp((r*(oracle_time-cdp.time))/31_557_600) -1) * dm)
		interest = int(cdp.debt * v) // dm
		cdp.add_debt(interest * SR // 100)
		val = -(interest * IR // price)
		cdp.add_collateral(val)
		# print("dt", oracle_time-cdp.time)
		# print("interest", interest)
		# print("collect c", -val)
		# print("price", price)
		# print("collect d", interest * SR // 100)
		CIT += interest * IR // price
		cdp.new_cd(cdp.collateral * 100 / cdp.debt)
		cdp.new_time(oracle_time)
	return cdp
	
def update_tax(cdp, price):
	cdp = add_tax(cdp, price)
	global IDP, AEC, CIT, TEC, oracle_time
	if AEC > 0 and cdp.debt <= epsilon(cdp.debt):
		if oracle_time != cdp.time:
			ec = cdp.collateral * 100 // cdp.acr 
			AGEC = ec * (oracle_time-cdp.time) # aggregated by this user
			dividends = IDP * AGEC // AEC
			AEC -= AGEC
			IDP -= dividends
			cdp.add_collateral(dividends)
			cdp.new_time(oracle_time)
	return cdp

# Contract functions

def liquidation(price, cr, lf):	
	print("liquidation")
	global TEC, table
	if table == []: return
	i = 0

	print_table()

	while i < len(table)-1:

		debtor = table.pop(len(table)-1)
		debtor = add_tax(debtor, price)

		if debtor.debt == 0:
			cdp_insert(debtor)
			return

		if debtor.collateral * price // debtor.debt >= cr  - epsilon(cr):
			cdp_insert(debtor)
			return
	
		if table[i].acr == 0:
			print("L2\n")
			print(table[i])
			cdp_insert(debtor)
			i += 1
			continue

		liquidator = table.pop(i)
		if liquidator.debt <= epsilon(liquidator.debt):
			TEC -= liquidator.collateral * 100 // liquidator.acr
		liquidator = update_tax(liquidator, price)

		liq_ccr = 9999999
		if liquidator.debt > 0: liq_ccr = liquidator.collateral * price / liquidator.debt

		if liquidator.debt > 0 and liq_ccr <= liquidator.acr + epsilon(liquidator.acr):
			print("L1\n")
			print(liquidator)
			i += 1
			cdp_insert(debtor)
			cdp_insert(liquidator)
			continue

		print("\nliquidator\n", liquidator)
		print("debtor\n", debtor)

		l = calc_lf(debtor, price, cr, lf)
		use_d = calc_val(debtor, liquidator, price, cr,l) # use debt
		use_c = min(use_d * 10000 // (price*(100-l)), debtor.collateral) # use col

		print("use d", use_d)
		print("use c", use_c)

		# if use_d <= 0: # used debt
		# 	print("L3")
		# 	print(liquidator)
		# 	i += 1
		# 	cdp_insert(debtor)
		# 	cdp_insert(liquidator)
		# 	if liquidator.debt <= epsilon(liquidator.debt):
		# 		TEC += liquidator.collateral * 100 // liquidator.acr
		# 	continue

		debtor.add_debt(-use_d)
		debtor.add_collateral(-use_c)
		liquidator.add_debt(use_d)
		liquidator.add_collateral(use_c)

		if debtor.debt <= epsilon(debtor.debt):
			debtor.new_cd(9999999)
		else:
			debtor.new_cd(debtor.collateral * 100 / debtor.debt)

		if liquidator.debt <= epsilon(liquidator.debt):
			TEC += liquidator.collateral * 100 // liquidator.acr
			liquidator.new_cd(9999999)
		else:
			liquidator.new_cd(liquidator.collateral * 100 / liquidator.debt)

		cdp_insert(liquidator)

		if debtor.debt > 0:
			# print("updating debtor")
			cdp_insert(debtor)
		else:
			print("removing debtor", debtor.id)
		i = 0
		# print(".\n")
	print("FAILED: END")

def redemption(amount, price, cr, rf):
	global time, table
	i = len(table)-1

	print_table()

	print("\n\nredeem", amount)

	while amount > epsilon(amount) and i != -1:
		cdp = table.pop(i)
		cdp = add_tax(cdp, price)

		if cdp.debt < 50000 + epsilon(50000):
			cdp_insert(cdp)
			i -= 1
			continue

		if cdp.collateral * price // cdp.debt < 100 - rf:
			cdp_insert(cdp)
			i -= 1
			continue

		if cdp.debt > amount:
			c = (amount * 100) // (price+rf)
			d = amount

			print(cdp)
			cdp.add_debt(-d)
			cdp.add_collateral(-c)
			cdp.new_cd(cdp.collateral * 100 / cdp.debt)
			cdp_insert(cdp)
			amount = 0

			print("redeem updating", d, c)
			print(cdp)
		else:
			d = cdp.debt
			c = (d * 100) // (price+rf)

			print(cdp)
			cdp.new_debt(0)
			cdp.add_collateral(-c)

			print("redeem removing", d, c)
			print(cdp)

			amount -= d
			i -= 1

	return

def reparametrize(id, c, d, price):
	global TEC, table, CR 
	cr = CR
	cdp = table.pop(cdp_index(id))
	print("reparam...", cdp)

	if cdp.acr != 0 and cdp.debt <= epsilon(cdp.debt):
		TEC -= cdp.collateral * 100 // cdp.acr

	cdp = update_tax(cdp, price)

	if d < 0:
		if cdp.debt + d > 50000 + epsilon(50000):
			cdp.add_debt(d)

	if c > 0:
		cdp.add_collateral(c)

	if c < 0:
		print("ccr", calc_ccr(cdp, price))
		print("----", cdp)
		if cdp.collateral + c > 5 + epsilon(5):
			if cdp.debt == 0:
				cdp.add_collateral(-c)
			else:
				if calc_ccr(cdp, price) < cr:
					print("reparam quit 1")
				else:
					m = (cr-100) * cdp.debt // price
					cdp.add_collateral(max(c, -m))

	if d > 0:
		print("ccr", calc_ccr(cdp, price))
		print("----", cdp)
		if cdp.debt == 0:
			cdp.add_debt(min(d, cdp.collateral * price // cr))
		else:
			if calc_ccr(cdp, price) < cr:
				print("reparam quit 2")
			else:
				val1 = cdp.collateral * price * 100 // (cr*cdp.debt)
				val2 = (val1 - 100)
				m = val2 * cdp.debt // 100
				cdp.add_debt(min(d, m))
	
	if cdp.debt != 0:
		cdp.new_cd(cdp.collateral * 100 / cdp.debt)
	else:
		cdp.new_cd(9999999)

	if cdp.acr != 0 and cdp.debt <= epsilon(cdp.debt):
		TEC += cdp.collateral * 100 // cdp.acr
	cdp_insert(cdp)
	print(cdp, "\n")

def change_acr(id, acr):
	global TEC, table, oracle_time, price
	if acr < CR or acr > 100000:
		return False

	cdp = table.pop(cdp_index(id))

	if cdp.acr == acr:
		cdp_insert(cdp)
		return False

	if cdp.acr != 0 and cdp.debt <= epsilon(cdp.debt):
		TEC -= (cdp.collateral * 100 // cdp.acr)

	cdp = update_tax(cdp, price)
	cdp.new_acr(acr)

	if cdp.acr != 0 and cdp.debt <= epsilon(cdp.debt):
		TEC += (cdp.collateral * 100 // cdp.acr)

	cdp_insert(cdp)

		
def update_round():
	global AEC, IDP, CIT, oracle_time, time, TEC, price
	val1 = TEC * (time - oracle_time)
	AEC += val1
	val2 = (CIT - (CIT * commission) // 100)
	IDP += val2
	CIT = 0
	oracle_time = time

# returns [time, [{action, failed?}], table]
def run_round(balance):
	global time, CR, LF, IR, r, SR, IDP, TEC, CIT, time, oracle_time, price, table

	LIQUIDATION = True
	REDEMPTION 	= True
	ACR 		= True
	REPARAM 	= True

	actions = []
	old_price = price
	length = len(table)
	if length == 0: return [time, actions]

	print(f"time: {time}")

	# acr requests get processed immediately
	if ACR and length > 1:
		k = 10
		for i in range(0, random.randint(1, length-1)):
			if cdp_index(i) != False:
				acr = random.randint(148, 300)
				failed = change_acr(i, acr)
				actions.append([["acr", i, acr], failed != False])
				k -= 1
			if k == 0:
				break

	# create reparam requests
	reparam_values = [] # [i, c, d, success]
	if REPARAM and length > 1:
		k = 10
		for i in range(0, random.randint(1, length-1)):
			idx = cdp_index(i) 
			if idx != False:
				c = random.randrange(-10000, 10000)
				d = random.randrange(-10000, 10000)

				# verify change with old price first (request creation step)
				cdp = table[idx]
				new_col = cdp.collateral + c
				new_debt = cdp.debt + d
				new_ccr = new_col * old_price / new_debt
				success = not (new_ccr < CR or new_debt < 50000 or new_debt < 50_0000 and new_debt != 0)
				reparam_values.append([i, c, d, success])
				k -= 1
			if k == 0:
				break

	new_price = random.randint(-price // 3, price // 3) if LIQUIDATION else 1
	if new_price == 0: new_price = 100
	price += new_price

	time_now()
	update_round()

	print(f"<<<<<<<<\nnew time: {time}, price: {price} (last price: {old_price})\n")

	# other requests get processed after oracle update

	if price < old_price:
		liquidation(price, 150, 10)
		length = len(table)
		if length == 0: return [time, actions]

	for values in reparam_values:
		i = values[0]
		c = values[1]
		d = values[2]
		success = values[3]
		if success: reparametrize(i, c, d, price)
		actions.append([["reparam", i, c, d], success])

	if REDEMPTION:
		v1 = random.randrange(100, 10_00_0000)
		success = v1 <= balance
		if success: redemption(v1, price, 150, 101)
		actions.append([["redeem", v1], success])

	# accrue taxes
	new_table = table.copy()
	table = []
	for cdp in new_table:
		if oracle_time - cdp.time > 2629800:
			cdp = add_tax(cdp, price)
		cdp_insert(cdp)

	return [time, actions]

def init():
	global table, IDP, TEC, AEC, CIT, time, oracle_time, price
	
	table = []
	IDP = 0 # insurance dividend pool
	TEC = 0 # total excess collateral
	AEC = 0 # aggregated excess collateral
	CIT = 0 # collected insurance tax
	time = 0 # initial time

	price = random.randint(500, 1000)

	x = 1
	d = random.randint(x, x * 3)
	l = random.randint(int(d * 2), int(d * 5))
	time_now()
	oracle_time = time

	gen(x, l)

	print(f"<<<<<<<<\nstart time: {time}, price: {price}\n")
