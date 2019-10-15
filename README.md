![esl project logo](https://raw.githubusercontent.com/INET-Complexity/ESL/master/project/logo/logo_card.png)


# ESL
The Economic Simulation Library (ESL) provides an extensive collection of tools to develop, test, analyse and calibrate economic and financial agent-based models. The library is designed to take advantage of different computer architectures. In order to facilitate rapid iteration during model development the library can use parallel computation. Economic models developed using the library can be deployed into large-scale distributed computing environments when working with large model instances and datasets. It provides routines to set up large-scale sampling experiments during the analysis and calibration process.

The library is developed to be used from Python, and is written in C++.  

# Features

## Models

### Agents

We aim to provide an extensive collection of agent classes that are commonly used in the literature.

* markets and market makers
* governments and central banks
* banks 
* investment funds
* pension funds
* households



#### Messages

By design, the agents use a messaging system to communicate. This allows us to analyse the agent-based models: we define the agent's behaviour to be the internal state changes and response messages generated by receiving a message of a specific type. 


### Market models

The library provides multiple market mechanisms that use the same messaging framework, meaning market mechanisms can to a limited extended be swapped and compared within the same economic model. We aim to provide simple models that allow, for example, to convert directly supply- and demand functions to an execution strategy, so that a Walrasian price-setters can be compared to order-driven markets such as limit order books. 

For release 0.0.2, we aim to provide the following market mechanisms.

* Walrasian price-setter, supporting market clearing for multiple assets using a numerical scheme, accelerated by automatic differentiation.

* An extensible limit order books with various order types, with FIX-message compatibility.

* Shapley-Shubik trading-posts. *Trade Using One Commodity as a Means of Payment*. Shapley, L. and Shubik, M. **Journal of Political Economy** 1977 vol: 85 (5) pp: 937-968.

* Common types of auctions

## Installation

As of version 0.0.1, we provide instructions for Ubuntu 18.04 LTS. Builds can be automated using the provided travis script.

Build with `N` processes:

```shell
mkdir build
cd build
cmake ../
make -jN
make test
```





## Examples

In the following minimal example, we create an empty model and populate it with two agents. We give the first agent `a` some cash, which it then transfers to agent `b` in the next timestep.
 
```python
import esl.agent
import esl.model
from esl.economics import cash, currencies

m = esl.model()

a = m.create(esl.agent)
b = m.create(esl.agent)

cash_amount = cash(10, currencies.USD)

a.cash_properties.insert(cash_amount)

a.create_message(transfer_cash(cash_amount), b)

m.step()

assert(b.cash_properties[esl.currencies.USD] == cash_amount)

```


## Testing

The library uses unit testing on both the native library (Boost.Test) and the Python bindings (pytest and Hypothesis).




## Acknowledgements

Work on this open-source project is in part funded by the J.P. Morgan AI Faculty Awards.



