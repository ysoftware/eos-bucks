# Copyright © Scruge 2019.
# This file is part of ScrugeX.
# Created by Yaroslav Erohin.

#!/bin/bash
python3 issuance.py &&
python3 liquidation.py &&
python3 close.py