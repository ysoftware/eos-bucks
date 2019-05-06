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
	time += 10_000_000 # random.randint(1000, 10000) * 1000 # maturity time up to 3 months
	return time

def epsilon(value): return value // 500

class CDP:
	def __init__(self, collateral, debt, cd, acr, id, time):
		self.collateral = collateral
		self.debt = debt
		self.cd = cd
		self.acr = acr 
		self.id = id
		self.time = time
	def __repr__(self):
		string = "c: " + str(self.collateral // 10000) + "."  + str(self.collateral % 10000)
		string2 = "d: " + ("0\t" if self.debt == 0 else (str(self.debt // 10000) + "." + str(self.debt % 10000)))
		return "#" + str(self.id)  + "\t" + string + "\t" + string2  + "\t" + ("acr: " + str(self.acr) + "\tcd: " + str(self.cd) if self.cd > 999999 else ("cd: " + str(int(self.cd)))) + "\t" + (("ec: " + str(self.collateral/self.acr)) if self.acr > 0 else ("")) + "\ttime: " + str(self.time//1_000_000) + "M"

	def add_debt(self,new_debt):
		self.debt = self.debt + new_debt
	def add_collateral(self, new_collateral):
		self.collateral = self.collateral + new_collateral
	def new_cd(self, cd_new):
		self.cd = cd_new
	def new_acr(self,acr_new):
		self.acr = acr_new
	def new_debt(self, debt_new):
		self.debt = debt_new
	def new_collateral(self, collateral_new):
		self.collateral = collateral_new
	def new_time(self, time_new):
		self.time = time_new		
	
# Functions for generation of sorted CDPs with random values

def generate_liquidators(k):
	global TEC, time
	rand = random.randrange(1000000,10000000,10000)
	rand2 = random.randint(150,200)
	liquidator = CDP(rand, 0, 9999999, rand2, 0, time)
	TEC += liquidator.collateral * 100 // liquidator.acr
	print("add excess", liquidator.collateral * 100 // liquidator.acr)
	liquidators = [liquidator]
	for i in range (0,k):
		helper = liquidators[i].acr
		rand2 = random.randint(helper+1,helper+2)
		liquidators.append(CDP(rand, 0, 9999999, rand2,i+1, time))
		liquidator = liquidators[len(liquidators)-1]
		TEC += liquidator.collateral * 100 // liquidator.acr
		print("add excess", liquidator.collateral * 100 // liquidator.acr)
	return liquidators

def generate_debtors(k, n):
	global time, price
	rand = random.randrange(1000000,10000000,10000) # collateral
	rand2 = random.randint(150,155) # cd
	ccr = rand2
	debtor = CDP(rand, 0, rand2, 0, k+1, time)
	debtor.add_debt(debtor.collateral * price // debtor.cd)
	debtor.new_cd(debtor.collateral * price // debtor.debt)
	debtors = [debtor]
	for i in range (k+1,n):
		rand = random.randrange(1000000,10000000,10000)
		helper = ccr
		rand2 = random.randint(helper+1, helper + 2)
		ccr = rand2
		debtor = CDP(rand, 0, rand2, 0, i+1, time)
		debtor.add_debt(debtor.collateral * price // debtor.cd)	
		debtor.new_cd(debtor.collateral * price // debtor.debt)
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
		return [cdp]
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
				if c * 10000 / acr > c2 * 10000 / acr2:
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
			if c * 10000000 / d  <  c2 * 10000000 / d2:
				table.insert(i+1, cdp)
				return 
		table.insert(0,cdp)
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
		for i in range(0,len(table)):
			print(table[i])	
			
# Helper functions for calculations

def calc_ccr(cdp, price):
	ccr = cdp.cd * price // 100
	return ccr
	
def calc_lf(cdp, price, cr, lf):
	ccr = calc_ccr(cdp, price)
	if ccr >= 100 + lf:
		l = lf
	elif ccr < 75:
		l = - 25
	else: 
		l = ccr - 100
	return l
	
def x_value(d, l, c, p):
		x = (100+l)*(750*d-5*c*p) // (50000-1500*l)
		return x
		
def calc_bad_debt(cdp, price, cr, lf):
	ccr = calc_ccr(cdp, price)
	val = (cr-ccr)*cdp.debt // 100+x_value(cdp.debt, lf, cdp.collateral, price)
	return val
		
def calc_val(cdp, cdp2, price, cr, lf):
	l = calc_lf(cdp, price, cr, lf)
	c = cdp2.collateral
	d = cdp2.debt
	acr = cdp2.acr
	v = calc_bad_debt(cdp, price, cr, l)
	v2 = (c*price-d*acr)*(100-l) // (acr*(100-l)-10000)
	return min(v,v2, cdp.debt)

# Taxes

def add_tax(cdp, price):
	global IDP, CIT, TEC, oracle_time

	if cdp.debt > epsilon(cdp.debt):

		# print("tax", cdp.id)

		interest = int(cdp.debt * (exp((r*(oracle_time-cdp.time))/(3.15576*10**7))-1))

		# print("c", interest * IR // price)
		# print("d", interest * SR // 100)
		# print("collected", CIT)

		
		cdp.add_debt(interest * SR // 100)
		cdp.add_collateral(-interest * IR // price)
		CIT += interest * IR // price
		cdp.new_cd(cdp.collateral * 100 // cdp.debt)
		cdp.new_time(oracle_time)

		# print("new collected", CIT)
		# print("---")
	return cdp
	
def update_tax(cdp, price):
	print("sell r", cdp)
	cdp = add_tax(cdp, price)
	global IDP, AEC, CIT, TEC, oracle_time
	if AEC > 0 and cdp.debt <= epsilon(cdp.debt):
		if oracle_time != cdp.time:
			ec = cdp.collateral * 100 // cdp.acr 
			AGEC = ec * (oracle_time-cdp.time) # aggregated by this user
			dividends = IDP * AGEC // AEC

			print("AEC", AEC)
			print("TEC", TEC)
			print("time", oracle_time)
			print("cdp time", cdp.time)
			print("insurer added col", dividends)
			AEC -= AGEC
			IDP -= dividends
			cdp.add_collateral(dividends)
			cdp.new_time(oracle_time)
	print(cdp, "\n---\n")
	return cdp

# Contract functions

def liquidation(price, cr, lf):	
		global TEC, table
		if table == []:
			return
		i = 0
		while table[i].cd * price >= cr * 100 + epsilon (cr*100):
			debtor = table.pop(len(table)-1)
			debtor = add_tax(debtor, price)
			if debtor.debt == 0:
				return
			if debtor.collateral * price // debtor.debt >= cr  - epsilon(cr):
				table.append(debtor)
				return 
			else:
				if table[i].acr == 0:
					if i == len(table)-1:
						table.append(debtor)
						return 
					else:
						i += 1
						table.append(debtor)
				elif table[i].cd * price <= table[i].acr * 100 + epsilon(table[i].acr * 100):
					i += 1
					table.append(debtor)
				else:
					liquidator = table.pop(i)
					if liquidator.debt <= epsilon(liquidator.debt):
						TEC -= liquidator.collateral * 100 // liquidator.acr
						print("remove excess", liquidator.collateral * 100 // liquidator.acr)
					liquidator = update_tax(liquidator, price)
					l = calc_lf(debtor, price, cr, lf)
					val = calc_val(debtor, liquidator, price, cr,l) 
					c = min(val * 10000 // (price*(100-l)),debtor.collateral)
					debtor.add_debt(-val)
					liquidator.add_debt(val)
					liquidator.add_collateral(c)
					debtor.add_collateral(-c)
					if debtor.debt <=  100:
						debtor.new_cd(9999999)
					else:
						debtor.new_cd(debtor.collateral * 100 // debtor.debt)
					if liquidator.debt <= epsilon(liquidator.debt):
						TEC += liquidator.collateral * 100 // liquidator.acr
						print("add excess", liquidator.collateral * 100 // liquidator.acr)
						liquidator.new_cd()
					else:
						liquidator.new_cd(liquidator.collateral * 100 // liquidator.debt)
					cdp_insert(liquidator)
					if debtor.debt >= 10:
						cdp_insert(debtor)

					if i == len(table):
						return
		return 

def redemption(amount, price, cr, rf):
	global time, table
	i = len(table)-1

	print("\n\nredeem", amount)

	while amount > epsilon(amount) and i != -1:
		cdp = table.pop(i)
		cdp = add_tax(cdp, price)

		if cdp.debt <= 50:
			cdp_insert(cdp)
			return
		else:
			if cdp.collateral * price // cdp.debt >= 100 - rf:
				if cdp.debt <= epsilon(cdp.debt): # duplicate check
					cdp_insert(cdp)
					return
				if amount < cdp.debt:
					print("redeem quantity", amount, cdp)
					print("using debt", amount, "col", (amount*100) // (price+rf))
					cdp.add_debt(-amount)
					cdp.add_collateral((amount*100) //(price+rf))
					amount = 0
					cdp.new_cd(cdp.collateral * 100 // cdp.debt)
					cdp_insert(cdp)
					print("reached end")
				else:
					d = cdp.debt
					print("redeem quantity", amount, cdp)
					print("using debt", cdp.debt, "col", (d*100) // (price+rf))
					cdp.new_debt(0)
					cdp.add_collateral((d*100) // (price+rf))
					amount -= d

					i -= 1
			else: 
				cdp_insert(cdp)
	print("redeem done 2")
	return

def reparametrize(id, c, d, price, old_price):
	global TEC, table, CR 
	cr = CR
	cdp = table.pop(cdp_index(id))
	print("reparam...", cdp)

	# verify change with old price first (request creation step)
	new_col = (cdp.collateral + c)
	new_debt = cdp.debt + d
	new_ccr = new_col * old_price / new_debt
	min_col = 5 * old_price * 10_000
	if new_ccr < CR and new_debt != 0 or new_col < min_col or new_debt < 50_0000 and new_debt != 0: 
		cdp_insert(cdp)
		return False

	if cdp.acr != 0 and cdp.debt <= epsilon(cdp.debt):
		TEC -= cdp.collateral * 100 // cdp.acr
		print("remove excess", cdp.collateral * 100 // cdp.acr)
	cdp = update_tax(cdp, price)

	if d < 0:
		if cdp.debt + d > 50000 + epsilon(50000):
			cdp.add_debt(d)
	if c > 0:
		cdp.add_collateral(c)
	if c < 0:
		if cdp.collateral + c > 5 + epsilon(5):
			if cdp.debt == 0:
				cdp.add_collateral(-c)
			else:
				if calc_ccr(cdp, price) < cr:
					return
				else:
					cdp.add_collateral(-(min(-c, (cr-100) * cdp.debt // price)))
	if d > 0:
		if cdp.debt == 0:
			cdp.add_debt(min(d, cdp.collateral * price // cr))
		else:
			if calc_ccr(cdp, price) < cr:
				return
			else:
				cdp.add_debt(min(d, (cdp.collateral * price * 100 // (cr*cdp.debt) - 100)*cdp.debt // 100))
	if cdp.debt != 0:
		cdp.new_cd(cdp.collateral * 100 // cdp.debt)
	else:
		cdp.new_cd(9999999)
	if cdp.acr != 0 and cdp.debt <= epsilon(cdp.debt):
		TEC += cdp.collateral * 100 // cdp.acr
		print("add excess", cdp.collateral * 100 // cdp.acr)
	cdp_insert(cdp)
	print(cdp)

def change_acr(id, acr):
	global TEC, table, oracle_time, price
	if acr < CR or acr > 100000:
		return False

	cdp = table.pop(cdp_index(id))
	print("change acr... to", acr, cdp)

	if cdp.acr == acr:
		cdp_insert(cdp)
		return False

	if cdp.acr != 0 and cdp.debt <= epsilon(cdp.debt):
		TEC -= cdp.collateral * 100 // cdp.acr
		print("remove excess", cdp.collateral * 100 // cdp.acr)

	cdp = update_tax(cdp, price)
	cdp.new_acr(acr)

	if cdp.acr != 0 and cdp.debt <= epsilon(cdp.debt):
		TEC += cdp.collateral * 100 // cdp.acr
		print("add excess", cdp.collateral * 100 // cdp.acr)

	cdp_insert(cdp)

		
def update_round():
	global AEC, IDP, CIT, oracle_time, time, TEC, price
	AEC += TEC * (time - oracle_time) 
	IDP += CIT * (100 - commission) // 100
	CIT = 0
	oracle_time = time

# returns [time, [{action, failed?}], table]
def run_round(balance):
	global time, CR, LF, IR, r, SR, IDP, TEC, CIT, time, oracle_time, price, table

	actions = []
	old_price = price
	price += 1 # = random.randint(100, 10000)

	# acr requests get processed immediately

	length = len(table)
	if length == 0: return [time, actions]

	print(f"time: {time}")

	k = 10
	for i in range(1, random.randint(1, length-1)):
		if cdp_index(i) != False:
			acr = random.randint(148, 300)
			failed = change_acr(i, acr)
			actions.append([["acr", i, acr], failed != False])
			k -= 1
		if k == 0:
			break

	time_now()
	update_round()

	print(f"<<<<<<<<\nnew time: {time}, price: {price} (last price: {old_price})\n")

	# other requests get processed after oracle update

	if price < old_price:
		liquidation(price, 150, 10)
		length = len(table)
		if length == 0: return [time, actions]

	# k = 10
	# for i in range(1, random.randint(1, length-1)):
	# 	if cdp_index(i) != False:
	# 		v1 = random.randrange(-100_0000, 1_000_0000)
	# 		v2 = random.randrange(-100_0000, 1_000_0000)
	# 		failed = reparametrize(i, v1, v2, price, old_price)
	# 		actions.append([["reparam", i, v1, v2], failed != False])
	# 		k -= 1
	# 	if k == 0:
	# 		break

	# v1 = random.randrange(0, 10_000_0000)
	# success = v1 <= balance
	# if success: redemption(v1, price, 150, 101)
	# actions.append([["redeem", v1], success])

	# accrue taxes
	for i in range(0, len(table)):
		if table[i].debt > epsilon(table[i].debt):
			cdp = table[i]
			if oracle_time - cdp.time > 2629800: # auto interest collection every month
				table[i] = add_tax(cdp, price)

	return [time, actions]

def init():
	global table, IDP, TEC, AEC, CIT, time, oracle_time, price
	
	table = []
	IDP = 0 # insurance dividend pool
	TEC = 0 # total excess collateral
	AEC = 0 # aggregated excess collateral
	CIT = 0 # collected insurance tax
	time = 0 # initial time

	price = random.randint(100, 1000)

	x = random.randint(3, 3)
	d = random.randint(x, x * 2)
	l = random.randint(int(d * 2), int(d * 5))
	time_now()
	oracle_time = time

	gen(2, 4)
	print_table()

	print(f"<<<<<<<<\nstart time: {time}, price: {price}\n")
