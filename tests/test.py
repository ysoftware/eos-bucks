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
	rand2 = random.randint(150,1000)/100
	liquidator = CDP(random.uniform(100,1000), 0, 9999999, rand2, 0)
	liquidators.append(liquidator)
	for i in range (0,k):
		rand2 = random.randint(150,liquidators[i].acr)/100
		liquidators.append(CDP(rand, 0, 9999999, rand2,i+1))
	return liquidators

def generate_debtors(k, n, price):
	debtors = []
	rand = random.randint(100,1000) 
	rand2 = random.randint(150,155)/100 
	debtor = CDP(rand, 0, random.uniform(1.5, 1.55),0, k+1)
	add_debt(debtor, debtor.collateral * price / debtor.cd)
	debtors.append(debtor)
	for i in range (k+1,n):
		rand2 = random.randint(debtors[i-k-1].cd,debtors[i-k-1].cd+0.05)/100 
		debtor = CDP(rand, 0, random.uniform(debtors[i-k-1].cd,debtors[i-k-1].cd+0.05), 0, i+1)
		debtor.add_debt(debtor.collateral * price / debtor.cd)
		debtors.insert(0, debtor)
	return debtors
		
		
def generate_table(k, n, price):
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
	def x(d, l, c, p):
		return (0.75*d*(1+lf)-0.5*c*p(1+lf))/(0.5-1.5*lf)
	return (cr-ccr)*cdp.debt+x(cdp.debt, lf, cdp.collateral, price)	
	
def calc_val(cdp, cdp2, price, cr, lf):
	l = calc_lf(cdp, price, cr, lf)
	c = cdp2.collateral
	d = cdp2.debt
	acr = cdp2.acr
	if cdp2.debt != 0:
		val = val = min(calc_bad_debt(cdp, price, cr, l),((c * price - d * acr) * (1-l))/(acr*(1-l)-1))
	else:
		val = val = min(calc_bad_debt(cdp, price, cr, l),(c * price * (1-l))/(acr*(1-l)-1))
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
				add_debt(debtor, -d)
				add_debt(liquidator,d)
				add_collateral(liquidator, d)
				add_collateral(debtor, -d)
				new_cd(debtor, debtor.collateral / debtor.debt)
				new_cd(liquidator, liquidator.collateral / liquidator.debt)
				cdp_insert(table, debtor)
				cdp_insert(table, liquidator)
		return table

def redemption(table, amount, price, cr, rf):
	len_table = len(table)
	while amount > 0:
		for i in range (len_table, -1, -1):
			cdp = table.pop(i)
			if cdp.cd * price >= 1 + rf:
				if amount <= cdp.debt:
					add_debt(cdp, -amount)
					add_collateral(cdp, -(amount)/(price-rf))
					amount = 0
					break
				else:
					d = cdp.debt
					add_debt(cdp, -d)
					add_collateral(cdp, -d/(price-rf))
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
	new_cd(cdp, cdp.collateral/cdp.debt)
	cdp.insert(table,cdp)
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

	
	
		
	

	