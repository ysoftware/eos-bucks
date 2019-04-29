import random
from math import exp
from math import ceil

CR = 150 # collateral ratio
LF = 10 # Liquidation Fee
IR = 20 # insurance ratio
SR = 100 - IR # savings ratio
r = 0.05 # interest rate
IDP = 0 # insurance dividend pool
TEC = 0 # total excess collateral
AEC = 0 # aggregated excess collateral
T = 1800 # round in seconds
CIT = 0 # collected insurance tax
commission = 20 # our commission
time = 0 # initial time


def time_now(time_last):
	global time
	time = random.randint(time_last, time_last + 7884 * 10 ** 3) # adding 3 months
	return time
 
 


class CDP:
	def __init__(self, collateral, debt, cd, acr, id, time):
		self.collateral = collateral
		self.debt = debt
		self.cd = cd
		self.acr = acr 
		self.id = id
		self.time = time
	def __repr__(self):
		string = "collateral: " + str(self.collateral // 10000) + ","  + str(self.collateral % 10000)
		string2 = " debt : " + str(self.debt // 10000) + ","  + str(self.debt % 10000)
		return "<CDP # ID: " + str(self.id)  + "; " + string + "; " + string2  + "; " + "cd: " + str(self.cd) + "% " + "; " + "acr: " +str(self.acr) + "%;" + " time: " + str(self.time)
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

def epsilon(value):
	return value // 500
		
	
# Functions for generation of sorted CDPs with random values

def generate_liquidators(k, t):
	global TEC
	rand = random.randrange(1000000,10000000,10000)
	rand2 = random.randint(150,155)
	liquidator = CDP(rand, 0, 9999999, rand2, 0, t)
	TEC += liquidator.collateral * 100 // liquidator.acr
	liquidators = [liquidator]
	for i in range (0,k):
		helper = liquidators[i].acr
		rand2 = random.randint(helper+1,helper+2)
		liquidators.append(CDP(rand, 0, 9999999, rand2,i+1, t))
		liquidator = liquidators[len(liquidators)-1]
		TEC += liquidator.collateral * 100 // liquidator.acr
	return liquidators

def generate_debtors(k, n, price, t):
	rand = random.randrange(1000000,10000000,10000)
	rand2 = random.randint(150,155)
	ccr = rand2
	debtor = CDP(rand, 0, rand2,0, k+1, t)
	debtor.add_debt(debtor.collateral * price // debtor.cd)
	debtor.new_cd(debtor.collateral * 100 // debtor.debt)
	debtors = [debtor]
	for i in range (k+1,n):
		rand = random.randrange(1000000,10000000,10000)
		helper = ccr
		rand2 = random.randint(helper+1, helper + 2)
		ccr = rand2
		debtor = CDP(rand, 0, rand2, 0, i+1, t)
		debtor.add_debt(debtor.collateral * price // debtor.cd)	
		debtor.new_cd(debtor.collateral * 100 // debtor.debt)
		debtors.insert(0, debtor)
	return debtors


def gen(k, n, price, t):
	liquidators = generate_liquidators(k, t)
	debtors = generate_debtors(k, n, price, t)
	return liquidators + debtors

# Function for inserting CDP into the table
	
def cdp_insert(table, cdp):
	if table == []:
		return [cdp]
	c = cdp.collateral
	d = cdp.debt
	acr = cdp.acr
	cd = cdp.cd
	len_table = len(table)
	if (d <= epsilon(d)) and acr == 0:
		return table
	elif (d <= epsilon(d)) and acr != 0:
		for i in range(0,len_table):
			cdp2 = table[i]
			d2 = cdp2.debt
			if d2 >= epsilon(d):
				table.insert(i,cdp)
				return table	
			acr2 = cdp2.acr
			cd2 = cdp2.cd
			if acr2 == 0:
				table.insert(i, cdp)
				return table
			else:
				if cd  > cd2:
					table.insert(i,cdp)
					return table
		table.append(cdp)
		return table
	else:
		for i in range(len(table)-1, -1, -1):
			cdp2 = table[i]
			d2 = cdp2.debt
			if d2 <= epsilon(d2):
				table.insert(i+1,cdp)
				return table
			c2 = cdp2.collateral
			acr2 = cdp2.acr
			cd2 = cdp2.cd
			if cd < cd2:
				table.insert(i+1, cdp)
				return table
		table.insert(0,cdp)
		return table
	
# Function for pulling out CDP from the table by querying its ID
def cdp_index(table, id):
	if len(table) == 0:
		return False
	for i in range(0, len(table)):
		if table[i].id == id:
			return i
			
			
			
def print_table(table):
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
	global IDP
	global AEC
	global CIT
	global TEC
	t = time
	if cdp.debt > epsilon(cdp.debt):	
		interest = ceil(cdp.debt * (exp((r*(t-cdp.time))/(3.154*10**7))-1))
		cdp.add_debt(interest * SR // 100)
		cdp.add_collateral(-interest * IR // price)
		CIT += interest * IR // price
		cdp.new_cd(cdp.collateral * 100 // cdp.debt)
		cdp.new_time(t)
	else:
		ec = cdp.collateral * 100 // cdp.acr 
		val = IDP * ec *(t-cdp.time) // (T * AEC)
		AEC -= ec *(t - cdp.time) // T
		cdp.add_collateral(val)
		cdp.new_time(t)
		TEC += val * 100 // cdp.acr
		IDP -= val
	return cdp
	
	
	
	
# Contract functions

def liquidation(table, price, cr, lf):	
		global TEC
		i = 0
		while table[i].cd * price >= cr * 100 + epsilon (cr*100):
			debtor = table.pop(len(table)-1)
			debtor = add_tax(debtor,price)
			print("\n")
			print("price")
			print(price)
			print("\n")
			print("debtor")
			print(debtor)
			print("\n")
			if debtor.debt == 0:
				return table
			if debtor.collateral * price // debtor.debt >= cr  - epsilon(cr):
				table.append(debtor)
				return table
			else:
				if table[i].acr == 0:
					if i == len(table)-1:
						table.append(debtor)
						return table
					else:
						i += 1
						table.append(debtor)
				elif table[i].cd * price <= table[i].acr * 100 + epsilon(table[i].acr * 100):
					i += 1
					table.append(debtor)
				else:
					liquidator = table.pop(i)
					TEC -= liquidator.collateral * 100 // liquidator.acr
					liquidator = add_tax(liquidator, price)
					print("\n")
					print("liquidator")
					print(liquidator)
					print("\n")
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
					if liquidator.debt <= 100:
						TEC += liquidator.collateral * 100 // liquidator.acr
						liquidator.new_cd(9999999)
					else:
						liquidator.new_cd(liquidator.collateral * 100 // liquidator.debt)
					table = cdp_insert(table, liquidator)
					if debtor.debt >= 10:
						table = cdp_insert(table, debtor)
					if i == len(table):
						return table
		return table

			

def redemption(table, amount, price, cr, rf):
	i = len(table)-1
	while amount > epsilon(amount) and i != -1:
			cdp = table.pop(i)
			cdp = add_tax(cdp, price)
			if cdp.cd * price >= (100 - rf)*100:
				if cdp.debt <= epsilon(cdp.debt):
					table = cdp_insert(table,cdp)
					return table
				elif amount < cdp.debt:
					cdp.add_debt(-amount)
					cdp.add_collateral(-(amount*100)//(price+rf))
					amount = 0
					cdp.new_cd(cdp.collateral * 100 // cdp.debt)
					table = cdp_insert(table,cdp)
					return table
				else:
					d = cdp.debt
					cdp.add_debt(-d)
					cdp.add_collateral((-d*100) // (price+rf))
					amount -= d
					if cdp.debt <= epsilon(cdp.debt):
						cdp.new_cd(9999999)
					else:
						cdp.new_cd(cdp.collateral * 100 // cdp.debt)
					table = cdp_insert(table,cdp)
					i -= 1
			else:
				table = cdp_insert(table,cdp)
				i -= 1
	return table
	
def reparametrize(table, id, c, d, cr, price):	
	global TEC
	cdp = table.pop(cdp_index(table, id))
	cdp = add_tax(cdp, price)
	if cdp.acr != 0 and cdp.debt == 0:
		TEC -= cdp.collateral * 100 // cdp.acr
	if d < 0:
		if cdp.debt + d > 50000 + epsilon(50000):
			cdp.add_debt(d)
	elif c > 0:
		cdp.add_collateral(c)
	elif c < 0:
		if cdp.collateral + c > 5 + epsilon(5):
			if cdp.debt == 0:
				cdp.add_collateral(-c)
			else:
				if calc_ccr(cdp, price) < cr:
					return table
				else:
					cdp.add_collateral(-(min(-c,(cr-100) * cdp.debt // price)))
	elif d > 0:
		if cdp.debt == 0:
			cdp.add_debt(min(d,cdp.collateral * price // cr))
		else:
			if calc_ccr(cdp, price) < cr:
				return table
			else:
				cdp.add_debt(min(d,(cdp.collateral * price * 100 // (cr*cdp.debt) - 100)*cdp.debt//100))
	if cdp.debt != 0:
		cdp.new_cd(cdp.collateral * 100 // cdp.debt)
	else:
		cdp.new_cd(9999999)
	if cdp.acr != 0 and cdp.debt == 0:
		TEC += cdp.collateral * 100 // cdp.acr
	table = cdp_insert(table,cdp)
	return table
	
def change_acr(table, id, acr, price):
	global TEC
	cdp = table.pop(cdp_index(table,id))
	cdp = add_tax(cdp, price)
	if cdp.acr != 0 and cdp.debt < 50000:
		TEC -= cdp.collateral * 100 // cdp.acr
	cdp.new_acr(acr)
	if cdp.acr != 0 and cdp.debt < 50000:
		TEC += cdp.collateral * 100 // cdp.acr
	table = cdp_insert(table, cdp)
	return table
	
	

		
			
			
			
			
def update_round(new_time, old_time):
	global AEC	
	global IDP
	global CIT
	AEC += TEC * (new_time - old_time) // T
	IDP += CIT * (100 - commission) // 100
	CIT = 0
	
	
	
#random testing

# liq, reparam, redeem, change_acr


def random_test(k, n, round):
	# globals, check at the top their mission
	global time
	global CR
	global LF
	global IR
	global r
	global SR
	global IDP
	global TEC
	global AEC
	global T
	global CIT
	global comission
	global time
	time = random.randint(1556463885,time_now(1556463885)) # generating random time between now and 3 months after
	price = random.randint(100, 1000)
	table = gen(k,n, price, time) # generating a table of cdps, where k is number of liquidators, and n-k is number of debtors
	length = len(table)
	AEC = TEC # Aggregated Excess Collateral is zero at the moment of initilization, therefore has to be updated once liquidators have been generated
	old_time = time
	for i in range(0, round): # round is the number of rounds for the random walk
		print("\n")
		print("round")
		print(i)
		print("\n")
		old_price = price
		price = random.randint(100, 1000)
		time = random.randint(old_time, time_now(old_time)) 
		if price < old_price:
			print("\n")
			print("liquidating")
			table = liquidation(table, price, 150, 10)
			print("done liquidating")
		update_round(time, old_time) 
		old_time = time
		k = 10
		for i in range(0, random.randint(0,length-1) ):
			if cdp_index(table, i) != False:
				print("\n")
				print("reparametrizing")
				print("\n")
				table = reparametrize(table, i, random.randrange(1000000,10000000,10000), random.randrange(1000000,10000000,10000), random.randint(150,1000), price)
				k -= 1
			if k == 0:
				break
		k = 10
		for i in range(0, random.randint(0, length - 1)):
			if cdp_index(table, i) != False:
				print("\n")
				print("reparametrizing ACR")
				print("\n")
				table = change_acr(table, i, random.randint(150,1000), price)
				k -= 1
			if k == 0:
				break
		print("\n")
		print("redeeming")
		print("\n")
		table = redemption(table, random.randrange(1000000,100000000,10000), price, 150, 101)
	print_table(table)
	# can add checks to ensure that insurance dividend pool is calculated rightly
		

random_test(1000,30000,20)		
		
#def redemption(table, amount, price, cr, rf):
			
		
	
	


# tester functions 


#def __init__(self, collateral, debt, cd, acr, id, time):

#table = gen(5,10,100)
#table = [CDP(1000000, 0, 9999999, 200, 0, 0), CDP(1000000, 500000, 200, 0, 1, 0)]
#TEC = 500000
#AEC = TEC




#def liquidation(table, price, cr, lf):	



#def add_tax(cdp, time, r, SR, IR, price):
#add_tax(table[2], 3.154*(10^7), 0.05, SR, IR, 100)





#print("\n")
#print_table(table)
#print("\n")

#table = cdp_insert(table, CDP(500000, 500000, 100, 0, 200, 0))




# print("\n")
# print_table(table)
# print("\n")


#def redemption(table, amount, price, cr, rf):
#table = redemption(table, 20000000, 40, 150, 1)

#def reparametrize(table, id, c, d, acr, cr, price):	
#table = reparametrize(table, 1, -10000000, 0, 0, 150, 100)
#table = reparametrize(table, 2, 10000, 500000, 0, 150, 100)


#print("\n")
#print_table(table)
#print("\n")

	


# buyers - people buying BUCK
# sellers - people selling BUCK
# people send EOS and BUCK to the contract
# when oracle updates, and after liquidation and all other things happen, exchange gets executed


#for buyer in buyers:
	#eos = buyer.eos.amount
	#for seller in sellers:
		#buck = seller.buck.amount
		#if eos * price > buck:
			#seller.buck.amount = 0
			#seller.eos.amount = buck / price
			#buyer.buck.amount = buck
			#buyers.eos.amount = eos - buck / price
		#elif eos * price < buck:
		#	seller.buck.amount = buck - eos * price
		#	seller.eos.amount = eos
			#buyer.buck.amount = eos * price
			#buyers.eos.amount = 0
			#break # choose next buyer
		#else:
			#seller.buck.amount = 0
			#seller.eos.amount = eos
			#buyer.buck.amunt = eos * price
			#buyer.eos.amount = 0
			#break # choose both next buyer and seller

		
			
			
			
			
		
	
		
	

	