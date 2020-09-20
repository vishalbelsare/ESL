#   \file   init__.py
# 
#   \brief
# 
#   \authors    Maarten P. Scholl
#   \date       2017-11-24
#   \copyright  Copyright 2017-2019 The Institute for New Economic Thinking,
#               Oxford Martin School, University of Oxford
# 
#               Licensed under the Apache License, Version 2.0 (the "License");
#               you may not use this file except in compliance with the License.
#               You may obtain a copy of the License at
# 
#                   http://www.apache.org/licenses/LICENSE-2.0
# 
#               Unless required by applicable law or agreed to in writing,
#               software distributed under the License is distributed on an "AS
#               IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
#               express or implied. See the License for the specific language
#               governing permissions and limitations under the License.
# 
#               You may obtain instructions to fulfill the attribution
#               requirements in CITATION.cff
#
import sys

# main module
try:
    from esl._esl import *
except ImportError:
    raise ModuleNotFoundError("Can't find ESL native module, did the compilation succeed?")

# submodules
from simulation import *