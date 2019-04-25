import random

class CDP:
	def __init__(self, collateral, debt, cd, acr, id):
		self.collateral = collateral
		self.debt = debt
		self.cd = cd
		self.acr = acr 
		self.id = id
	def __repr__(self):
		string = "collateral: " + str(self.collateral // 10000) + ","  + str(self.collateral % 10000)
		string2 = " debt : " + str(self.debt // 10000) + ","  + str(self.debt % 10000)
		return "<CDP # ID: " + str(self.id)  + "; " + string + "; " + string2  + "; " + "cd: " + str(self.cd) + "% " + "; " + "acr: " +str(self.acr) + "%"
		#"<CDP # ID%s, " + string + " debt: %s, cd: %s, acr: %s>" % (self.id, self.collateral, self.debt, self.cd, self.acr)
		#return "<CDP # ID%s, collateral: %s, debt: %s, cd: %s, acr: %s>" % (self.id, self.collateral, self.debt, self.cd, self.acr)
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
		
	
# Functions for generation of sorted CDPs with random values

def generate_liquidators(k):
	liquidators = []
	rand = random.randrange(1000000,10000000,10000)
	rand2 = random.randint(150,155)
	liquidator = CDP(rand, 0, 9999999, rand2, 0)
	liquidators.append(liquidator)
	for i in range (0,k):
		helper = liquidators[i].acr
		rand2 = random.randint(helper,helper+50)
		liquidators.append(CDP(rand, 0, 9999999, rand2,i+1))
	return liquidators

def generate_debtors(k, n, price):
	debtors = []
	rand = random.randrange(1000000,10000000,10000)
	rand2 = random.randint(150,155)
	debtor = CDP(rand, 0, rand2,0, k+1)
	debtor.add_debt(debtor.collateral * price / debtor.cd)
	debtors.append(debtor)
	for i in range (k+1,n):
		rand = random.randrange(1000000,10000000,10000)
		helper = debtors[0].cd
		rand2 = random.randint(helper, helper +50)
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
				if c * 100 / acr > c2 * 100 / acr2:
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
				if c * 100 / d >= c2 * 100 / d2:
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
	return "Not found"
			
			
			
def print_table(table):
	if len(table) == 0:
		print("table is empty")
	else:
		for i in range(0,len(table)):
			print table[i]
			print "\n"			
			
			

		
			


		

# Helper functions for calculations

def calc_ccr(cdp, price):
	ccr = cdp.collateral * price / cdp.debt
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
		x = (100+l)*(750*d-5*c*p)/(50000-1500*l)
		return x
		
def calc_bad_debt(cdp, price, cr, lf):
	ccr = calc_ccr(cdp, price)
	val = (cr-ccr)*cdp.debt/100+x_value(cdp.debt, lf, cdp.collateral, price)
	return val
	
	
def calc_val(cdp, cdp2, price, cr, lf):
	l = calc_lf(cdp, price, cr, lf)
	c = cdp2.collateral
	d = cdp2.debt
	acr = cdp2.acr
	v = calc_bad_debt(cdp, price, cr, l)
	v2 = (c*price-d*acr)*(100-l)/(acr*(100-l)-10000)
	return min(v,v2, cdp.debt)
	
# Contract functions

def liquidation(table, price, cr, lf):	
		i = 0
		while table[i].cd * price >= cr * 100 :
			if i == (len(table) - 1):
				return table
			elif table[i].cd * price <= table[i].acr * 100:
				i += 1
			else:
				debtor = table.pop(len(table)-1)
				if debtor.cd * price >= cr * 100:
					table.append(debtor)
					return table
				else:
					if table[i].acr == 0:
						i += 1
					else:
						liquidator = table.pop(i)
						l = calc_lf(debtor, price, cr, lf)
						val = calc_val(debtor, liquidator, price, cr,l)
						c = min(val * 10000 / (price*(100-l)),debtor.collateral)
						debtor.add_debt(-val)
						liquidator.add_debt(val)
						liquidator.add_collateral(c)
						debtor.add_collateral(-c)
						if debtor.debt == 0:
							debtor.new_cd(9999999)
						else:
							debtor.new_cd(debtor.collateral * 100 / debtor.debt)
						liquidator.new_cd(liquidator.collateral * 100 / liquidator.debt)
						table = cdp_insert(table, liquidator)
						table = cdp_insert(table, debtor)
		return table

			

def redemption(table, amount, price, cr, rf):
	i = len(table)-1
	while amount > 0 and i != -1:
			cdp = table.pop(i)
			if cdp.cd * price >= (100 + rf)*100:
				if cdp.debt == 0:
					table = cdp_insert(table,cdp)
					i -= 1
				elif amount <= cdp.debt:
					cdp.add_debt(-amount)
					cdp.add_collateral(-(amount*100)/(price+rf))
					amount = 0
					cdp.new_cd(cdp.collateral * 100 / cdp.debt)
					table = cdp_insert(table,cdp)
					return table
				else:
					d = cdp.debt
					cdp.add_debt(-d)
					cdp.add_collateral((-d*100)/(price+rf))
					amount -= d
					cdp.new_cd(cdp.collateral * 100 / cdp.debt)
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
				cdp.add_collateral(-(min(-c,(cr-100) * cdp.debt / price)))
	if d > 0:
		if cdp.debt == 0:
			cdp.add_debt(min(d,cdp.collateral * price / cr))
		else:
			if calc_ccr(cdp, price) < cr:
				return table
			else:
				cdp.add_debt(min(d,(cdp.collateral * price * 100 / (cr*cdp.debt) - 100)*cdp.debt/100))
	if cdp.debt != 0:
		cdp.new_cd(cdp.collateral * 100/cdp.debt)
	else:
		cdp.new_cd(9999999)
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
#table = gen(5,10,100)
table = [CDP(10000000,0,9999999,200,0), CDP(1000000,0,200,0,1)]


print_table(table)

print("\n")
print("\n")

#table = liquidation(table, 50, 150, 10)
#table = redemption(table, 200000, 100, 150, 1)
table = reparametrize(table, 1, 0, 500000, 0, 150, 100)

print_table(table)

	
		
	

	