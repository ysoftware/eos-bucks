import random

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
		
	
# Functions for generation of sorted CDPs with random values

def generate_liquidators(k):
	liquidators = []
	rand = random.randint(100,1000)
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
	rand = random.randint(100,1000) 
	rand2 = float(random.randint(150,155))/100 
	debtor = CDP(rand, 0, rand2,0, k+1)
	debtor.add_debt(debtor.collateral * price / debtor.cd)
	debtors.append(debtor)
	for i in range (k+1,n):
		rand = random.randint(100,1000)
		helper = int(debtors[0].cd*100)
		rand2 = (float(random.randint(helper, helper +50)))/100 
		debtor = CDP(rand, 0, rand2, 0, i+1)
		debtor.add_debt(debtor.collateral * price / debtor.cd)
		debtors.insert(0, debtor)
	return debtors


def gen(k, n, price):
	liquidators = generate_liquidators(k)
	debtors = generate_debtors(k, n, price)
	return liquidators + debtors




# Function for inserting CDP into the table
	
def cdp_insert(table, cdp):
	c = cdp.collateral
	d = cdp.debt
	acr = cdp.acr
	cd = cdp.cd
	len_table = len(table)
	if d == 0:
		for i in range(0, len_table):
			cdp2 = table[i]
			c2 = cdp2.collateral
			d2 = cdp2.debt
			acr2 = cdp2.acr
			if d2 != 0 or c/acr > c2/acr2:
				table.insert(i, cdp)
				return table
	else:
		for i in range (len_table - 1, -1, -1):
			cdp2 = table[i]
			c2 = cdp2.collateral
			d2 = cdp2.debt
			acr2 = cdp2.acr
			cd2 = cdp2.cd
			if d2 == 0 or cd <= cd2:
				table.insert(i+1,cdp)
				return table
	
# Function for pulling out CDP from the table by querying its ID
def cdp_index(table, id):
	for i in range(0, len(table)):
		if table[i].id == id:
			return i
			
			
			
def print_table(table):
	for i in range(0,len(table)):
		print table[i]
		print "\n"			
			
			

		
			


		

# Helper functions for calculations

def calc_ccr(cdp, price):
	return cdp.cd * price
	
def calc_lf(cdp, price, cr, lf):
	ccr = calc_ccr(cdp, price)
	if ccr >= 1+lf:
		l = lf
	elif ccr < 0.75:
		l = -0.25
	else: 
		l = ccr - 1
	return l
	
		
def calc_bad_debt(cdp, price, cr, lf):
	ccr = calc_ccr(cdp, price)
	def x_value(d, l, c, p):
		x = (0.75 * d * (1+l) - 0.5*c*p*(1+l))/(0.5-1.5*l)
		return x
	return (cr-ccr)*cdp.debt+x_value(cdp.debt, lf, cdp.collateral, price)	
	
def calc_val(cdp, cdp2, price, cr, lf):
	l = calc_lf(cdp, price, cr, lf)
	c = cdp2.collateral
	d = cdp2.debt
	acr = cdp2.acr
	if cdp2.debt != 0:
		val = min(calc_bad_debt(cdp, price, cr, l),((c * price - d * acr) * (1-l))/(acr*(1-l)-1))
	else:
		val = min(calc_bad_debt(cdp, price, cr, l), c * price * (1-l)/(acr*(1-l)-1))
	return val
	
# Contract functions

def liquidation(table, price, cr, lf):
		len_table = len(table)
		while table[0].cd * price >= cr:
			debtor = table.pop(len_table-1)
			if debtor.cd * price >= cr:
				table.append(debtor)
				break
			else:
				liquidator = table.pop(0)
				l = calc_lf(debtor, price, cr, lf)
				d = calc_val(debtor, liquidator, price, cr,l)
				c = d / (price*(1-l))
				debtor.add_debt(-d)
				liquidator.add_debt(d)
				liquidator.add_collateral(c)
				debtor.add_collateral(-c)
				debtor.new_cd(debtor.collateral / debtor.debt)
				liquidator.new_cd(liquidator.collateral / liquidator.debt)
				table = cdp_insert(table, debtor)
				table = cdp_insert(table, liquidator)
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
table = gen(20,40,1)
a = CDP(450, 0, 9999999, 7.8, 100)
b = CDP(150, 50, 3, 0, 200)
table = cdp_insert(table,a)
table = cdp_insert(table,b)
print("Initialized")
print_table(table)			
table = liquidation(table, 0.5, 1.5, 0.1)	
print("Liquidated")
print_table(table)		
	
		
	

	