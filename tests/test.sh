# Copyright © Scruge 2019.
# This file is part of ScrugeX.
# Created by Yaroslav Erohin.

#!/bin/bash
python3 issuance.py &&
python3 close.py && 
python3 reparam.py &&
python3 transfer.py &&
python3 tax.py && 
python3 savings.py

# python3 redeem.py &&