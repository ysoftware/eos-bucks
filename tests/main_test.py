class CDP:
	def __init__(self, collateral, debt, cd, acr):
		self.collateral = collateral
		self.debt = debt
		self.cd = cd
		self.acr = acr 
	def add_debt(self,new_debt):
		self.debt = self.debt + new_debt
	def add_сollateral(self,new_collateral):
		self.collateral = self.collateral + new_collateral
	def new_cd(self, cd_new):
		self.cd = cd_new
	def new_acr(self,acr_new)
		self.acr = acr_new
		
	
# Functions for generation of sorted CDPs with random values

def generate_debtors(n, price):
	debtors = [CDP(rand, 0, random.uniform(1.5, 1.55),0)]
	debtors[0].add_debt(debtors[0].collateral * price / debtors[0].cd)
	for i in range (0,n):
		debtors.append = CDP(rand, 0, random.uniform(debtors[i].cd,debtors[i].cd+0.05),0)
		debtors[i+1].add_debt(debtors[i+1].collateral * price / debtors[i+1].cd)
	return debtors
		
def generate_liquidators(n):
	liquidators = [CDP(rand, 0, 9999999, random.uniform(1.5, 10.0))]
	for i in range (0,n):
		liquidators.append = CDP(rand, 0, 0, random.uniform(1.5, liquidators[i].acr))
	return liquidators
		
		
def generate_liquidators_and_debtors(k, n, price):
	liquidators = generate_liquidators(k)
	debtors = generate_debtors(n, price)
	return liquidators.extend(debtors)
	

# Insert function 
	
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
			if d2 != 0 OR c/acr > c2/acr2:
				table.insert(i, cdp)
	else:
		for i in range (len_table - 1, -1, -1):
			cdp2 = table[i]
			c2 = cdp2.collateral
			d2 = cdp2.debt
			acr2 = cdp2.acr
			cd2 = cdp2.cd
				if d2 == 0 OR cd <= cd2:
					table.insert(i+1,cdp)
	return table
			
		
		

		

# Helper functions for calculations

def calc_ccr(cdp, price):
	return cdp.cd * price
	
def calc_l(cdp, price, cr, lf):
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
	l = calc_l(cdp, price, cr, lf)
	c = cdp2.collateral
	d = cdp2.debt
	acr = cdp2.acr
	if cdp2.debt != 0:
		val = val = min(calc_bad_debt(cdp, price, cr, l),((c * price - d * acr) * (1-l))/(acr*(1-l)-1))
	else:
		val = val = min(calc_bad_debt(cdp, price, cr, l),(c * price * (1-l))/(acr*(1-l)-1))
	return val
	
# Contract functions

def liquidation(table, price, cr, lf)
		len_table = len(table)
		while table[0].cd * price >= cr:
			debtor = table.pop(len_table-1)
			if debtor.cd * price >= cr:
				table.append(debtor)
				break
			else:
				liquidator = table.pop(0)
				l = calc_l(debtor, price, cr, lf)
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
	
			
			
# Round 1 starts	
old_price = price
price = price / random.uniform(0.5, 2.0)

update(buck, price)

# Liquidation


				
				

				
			
			
			
	
	
		
		 
		
	
	
	
	
# check that it was liquidated as much as possible

# repararam

# redemp


	

	
	
		
	

	