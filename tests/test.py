import random
from math import floor

class CDP:
	def __init__(self, collateral, debt, cd, acr, id):
		self.collateral = collateral
		self.debt = debt
		self.cd = cd
		self.acr = acr 
		self.id = id
	def __repr__(self):
		return "<CDP # ID%s, collateral: %s, debt: %s, cd: %s, acr: %s>" % (self.id, self.collateral, self.debt, self.cd, self.acr)
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
		
	
# Functions for generation of sorted CDPs with random values

def generate_liquidators(k):
	liquidators = []
	rand = float(random.randint(100,1000))
	rand2 = float(random.randint(150,155))/100
	liquidator = CDP(rand, 0, 9999999, rand2, 0)
	liquidators.append(liquidator)
	for i in range (0,k):
		helper = int(liquidators[i].acr * 100)
		rand2 = float(random.randint(helper,helper+50))/100
		liquidators.append(CDP(rand, 0, 9999999, rand2,i+1))
	return liquidators

def generate_debtors(k, n, price):
	debtors = []
	rand = float(random.randint(100,1000) )
	rand2 = float(random.randint(150,155))/100 
	debtor = CDP(rand, 0, rand2,0, k+1)
	debtor.add_debt(round(debtor.collateral * price / debtor.cd,3))
	debtors.append(debtor)
	for i in range (k+1,n):
		rand = float(random.randint(100,1000))
		helper = int(debtors[0].cd*100)
		rand2 = (float(random.randint(helper, helper +50)))/100 
		debtor = CDP(rand, 0, rand2, 0, i+1)
		debtor.add_debt(round(debtor.collateral * price / debtor.cd,3))
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
	if d == 0 and acr == 0:
		return table
	elif d == 0:
		for i in range(0,len_table):
			cdp2 = table[i]
			c2 = cdp2.collateral
			d2 = cdp2.debt
			acr2 = cdp2.acr
			cd2 = cdp2.cd
			if d2 != 0:
				table.insert(i,cdp)
				return table
			elif acr2 == 0:
				table.insert(i, cdp)
				return table
			else:
				if c/acr > c2/acr2:
					table.insert(i,cdp)
					return table
		table.append(cdp)
		return table
	else:
		for i in range(0, len_table):
			cdp2 = table[i]
			c2 = cdp2.collateral
			d2 = cdp2.debt
			acr2 = cdp2.acr
			cd2 = cdp2.cd
			if d2 != 0:
				if c/d >= c2/d2:
					table.insert(i,cdp)
					return table
		table.append(cdp)
		return table
	table.append(cdp)
	return table
	
# Function for pulling out CDP from the table by querying its ID
def cdp_index(table, id):
	for i in range(0, len(table)):
		if table[i].id == id:
			return i
	print("\n")
	print("holy fuck")
	print("\n")
	return table
			
			
			
def print_table(table):
	if len(table) == 0:
		print("table is empty")
	else:
		for i in range(0,len(table)):
			print table[i]
			print "\n"			
			
			

		
			


		

# Helper functions for calculations

def calc_ccr(cdp, price):
	ccr = (float(cdp.collateral) / cdp.debt) * price
	return ccr
	
def calc_lf(cdp, price, cr, lf):
	ccr = calc_ccr(cdp, price)
	if ccr >= 1+lf:
		l = lf
	elif ccr < 0.75:
		l = -0.25
	else: 
		l = ccr - 1
	return l
	
def x_value(d, l, c, p):
		x = (0.75 * d * (1+l) - 0.5*c*p*(1+l))/(0.5-1.5*l)
		return x
		
def calc_bad_debt(cdp, price, cr, lf):
	ccr = calc_ccr(cdp, price)
	val = (cr-ccr)*cdp.debt+x_value(cdp.debt, lf, cdp.collateral, price)	
	return val
	
	
def calc_val(cdp, cdp2, price, cr, lf):
	l = calc_lf(cdp, price, cr, lf)
	c = cdp2.collateral
	d = cdp2.debt
	acr = cdp2.acr
	v = calc_bad_debt(cdp, price, cr, l)
	v2 = (c*price-d*acr)*(1-l)/(acr*(1-l)-1)
	return min(v,v2, cdp.debt)
	
# Contract functions

def liquidation(table, price, cr, lf):	
		i = 0
		while table[i].cd * price >= cr :
			if i == (len(table) - 1):
				return table
			elif table[i].cd * price <= table[i].acr:
				i += 1
			else:
				debtor = table.pop(len(table)-1)
				if debtor.cd * price >= cr:
					table.append(debtor)
					return table
				else:
					if table[i].acr == 0:
						i += 1
					else:
						liquidator = table.pop(i)
						l = calc_lf(debtor, price, cr, lf)
						val = calc_val(debtor, liquidator, price, cr,l)
						c = min(val / (price*(1-l)),debtor.collateral)
						debtor.add_debt(round(-val,3))
						liquidator.add_debt(round(val,3))
						liquidator.add_collateral(floor(c))
						debtor.add_collateral(floor(-c))
						if debtor.debt <=0.01:
							debtor.new_cd(999999999)
						else:
							debtor.new_cd(round(debtor.collateral / debtor.debt,2))
						liquidator.new_cd(round(liquidator.collateral / liquidator.debt,2))
						table = cdp_insert(table, liquidator)
						table = cdp_insert(table, debtor)
		return table

			

def redemption(table, amount, price, cr, rf):
	len_table = len(table)
	while amount > 0:
		for i in range (len_table, -1, -1):
			cdp = table.pop(i)
			if cdp.cd * price >= 1 + rf:
				if amount <= cdp.debt:
					cdp.add_debt(-amount)
					cdp.add_collateral(-(amount)/(price-rf))
					amount = 0
					table = cdp_insert(table,cdp)
				else:
					d = cdp.debt
					cdp.add_debt(-d)
					cdp.add_collateral(-d/(price-rf))
					amount -= d
	return table
	
def reparametrize(table, id, c, d, acr, cr, price):	
	cdp = table.pop(cdp_index(table, id))
	new_acr(cdp,acr)
	if d < 0:
		new_debt(cdp,-d)
	if c > 0:
		new_collateral(cdp, c)
	if c < 0:
		new_collateral(cdp, min(cr/(cdp.collateral * price / cdp.debt),c/cdp.collateral)*cdp.collateral)
	if d > 0:
		new_debt(cdp,min(((cdp.collateral * price / cdp.debt)/cr-1)*cdp.debt,d))
	cdp.new_cd(cdp.collateral/cdp.debt)
	table = cdp_insert(table,cdp)
	return table
	

		
			
			
			
			
# Round 1 starts	
#old_price = price
#price = price / random.uniform(0.5, 2.0)

#update(buck, price)

# Liquidation


				
				

				
			
			
			
	
	
		
		 
		
	
	
	
	
# check that it was liquidated as much as possible

# repararam

# redemp



# tester functions
table = gen(25,50,1)


print_table(table)

print("\n")
print("\n")





table = liquidation(table, 0.5, 1.5, 0.1)

print_table(table)



	
		
	

	