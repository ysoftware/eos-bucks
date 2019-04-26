import random

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
		self.new_time = time_new

def epsilon(value):
	return value // 500000
		
	
# Functions for generation of sorted CDPs with random values

def generate_liquidators(k):
	liquidators = []
	rand = random.randrange(1000000,10000000,10000)
	rand2 = random.randint(150,200)
	liquidator = CDP(rand, 0, 9999999, rand2, 0, 0)
	liquidators.append(liquidator)
	for i in range (0,k):
		helper = liquidators[i].acr
		# rand = random.randrange(1000000,10000000,10000)
		rand2 = random.randint(helper+1,helper+2)
		liquidators.append(CDP(rand, 0, 9999999, rand2,i+1, 0))
	return liquidators

def generate_debtors(k, n, price):
	debtors = []
	rand = random.randrange(1000000,10000000,10000)
	rand2 = random.randint(150,170)
	debtor = CDP(rand, 0, rand2,0, k+1, 0)
	debtor.add_debt(debtor.collateral * price // debtor.cd)
	debtors.append(debtor)
	for i in range (k+1,n):
		rand = random.randrange(1000000,10000000,10000)
		helper = debtors[0].cd
		rand2 = random.randint(helper, helper + 1)
		debtor = CDP(rand, 0, rand2, 0, i+1, 0)
		debtor.add_debt(debtor.collateral * price // debtor.cd)	
		debtors.insert(0, debtor)
	return debtors


def gen(k, n, price):
	liquidators = generate_liquidators(k)
	debtors = generate_debtors(k, n, price)
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
	for i in range(0, len(table)):
		if table[i].id == id:
			return i
	return "Not found"
			
			
			
def print_table(table):
	if len(table) == 0:
		print("table is empty")
	else:
		for i in range(0,len(table)):
			print(table[i])	
			
			

		
			


		

# Helper functions for calculations

def calc_ccr(cdp, price):
	ccr = cdp.collateral * price // cdp.debt
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
		print("x val", x)
		return x
		
def calc_bad_debt(cdp, price, cr, lf):
	ccr = calc_ccr(cdp, price)
	val = (cr-ccr)*cdp.debt // 100+x_value(cdp.debt, lf, cdp.collateral, price)
	print("bad debt", val)
	return val
	
	
def calc_val(cdp, cdp2, price, cr, lf):
	l = calc_lf(cdp, price, cr, lf)
	c = cdp2.collateral
	d = cdp2.debt
	acr = cdp2.acr
	v = calc_bad_debt(cdp, price, cr, l)
	v2 = (c*price-d*acr)*(100-l) // (acr*(100-l)-10000)
	print("v2", v2)
	print("val", min(v,v2, cdp.debt))
	return min(v,v2, cdp.debt)
	
	
# Contract functions

def liquidation(table, price, cr=150, lf=10):	
		i = 0
		while table[i].cd * price >= cr * 100 + epsilon (cr*100) :
			debtor = table.pop(len(table)-1)
			#print("\n")
			print("debtor")
			print(debtor)
			#print("\n")
			if debtor.cd * price >= cr * 100 + epsilon (cr*100):
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
					#print("\n")
					print("liquidator")
					print(liquidator)
					#print("\n")
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
						liquidator.new_cd(9999999)
					else:
						liquidator.new_cd(liquidator.collateral * 100 // liquidator.debt)
					table = cdp_insert(table, liquidator)
					if debtor.debt >= 10:
						table = cdp_insert(table, debtor)
					#if table[i].cd * price <= table[i].acr * 100 + (table[i].acr  // 20):
						#i += 1
					if i == len(table):
						return table
		return table

			

def redemption(table, amount, price, cr, rf):
	i = len(table)-1
	while amount > epsilon(amount) and i != -1:
			cdp = table.pop(i)
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
	
def reparametrize(table, id, c, d, acr, cr, price):	
	cdp = table.pop(cdp_index(table, id))
	cdp.new_acr(acr)
	if d < 0:
		cdp.add_debt(d)
	if c > 0:
		cdp.add_collateral(c)
	if c < 0:
		if cdp.debt == 0:
			cdp.add_collateral(-c)
		else:
			if calc_ccr(cdp, price) < cr:
				return table
			else:
				cdp.add_collateral(-(min(-c,(cr-100) * cdp.debt // price)))
	if d > 0:
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
	table = cdp_insert(table,cdp)
	return table
	

		
			
			
			
			
# Round 1 starts	
#old_price = price
#price = price // random.uniform(0.5, 2.0)

#update(buck, price)

# Liquidation


				
				

				
			
			
			
	
	
		
		 
		
	
	
	
	
# check that it was liquidated as much as possible

# repararam

# redemp
#def __init__(self, collateral, debt, cd, acr, id, time):


# tester functions 
# price = 100, generates 25 liquidators and 25 debtors
# table = [CDP(10000000, 0, 9999999, 200, 0, 0), CDP(10000000, 5000000, 200, 0, 1, 0), CDP(10000000, 5000000, 200, 0, 2, 0), CDP(10000000, 5000000, 200, 0, 3, 0), CDP(10000000, 5000000, 200, 0, 4, 0), CDP(10000000, 5000000, 200, 0, 5, 0)]
# table = gen(2,20,100)



# print("\n")
# print_table(table)
# print("\n")
# #table = cdp_insert(table, CDP(500000, 500000, 100, 0, 200, 0))


# #def liquidation(table, price, cr, lf):	
# table = liquidation(table, 40, 150, 10)

# print("\n")
# print_table(table)
# print("\n")


#def redemption(table, amount, price, cr, rf):
#table = redemption(table, 20000000, 40, 150, 1)

#table = reparametrize(table, 1, 0, 500000, 0, 150, 100)


#print("\n")
#print_table(table)
#print("\n")

	
		
	

	