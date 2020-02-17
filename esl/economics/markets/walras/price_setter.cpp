/// \file   auctioneer.cpp
///
/// \brief
///
/// \authors    Maarten P. Scholl
/// \date       2018-07-19
/// \copyright  Copyright 2017-2019 The Institute for New Economic Thinking,
/// Oxford Martin School, University of Oxford
///
///             Licensed under the Apache License, Version 2.0 (the "License");
///             you may not use this file except in compliance with the License.
///             You may obtain a copy of the License at
///
///                 http://www.apache.org/licenses/LICENSE-2.0
///
///             Unless required by applicable law or agreed to in writing,
///             software distributed under the License is distributed on an "AS
///             IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
///             express or implied. See the License for the specific language
///             governing permissions and limitations under the License.
///
///             You may obtain instructions to fulfill the attribution
///             requirements in CITATION.cff
///

#include <esl/economics/markets/walras/price_setter.hpp>

#include <stan/services/optimize/lbfgs.hpp>


#include <algorithm>
#include <numeric>


#include <esl/economics/cash.hpp>
#include <esl/economics/markets/walras/quote_message.hpp>
#include <esl/economics/markets/walras/tatonnement.hpp>
#include <esl/economics/owner.hpp>
#include <esl/simulation/time.hpp>

// template<typename floating_t_ = double, typename integer_t_ = size_t>
std::vector<int> c(const std::vector<double> &fractions, int sum)
{
    (void)sum;

    double sum_ = 0;
    int near_   = 0;
    std::vector<int> result_;
    for(const auto &f : fractions) {
        sum_ += f;
        near_ += round(f);
        result_.push_back(round(abs(f)));
    }

    int target_ = round(sum_);

    if(near_ == target_) {
        return result_;
    }


    std::vector<size_t> indices_(fractions.size());
    iota(indices_.begin(), indices_.end(), 0);

    sort(indices_.begin(), indices_.end(), [&fractions](size_t i1, size_t i2) {
        return (fractions[i1] - std::nearbyint(fractions[i1]))
               > (fractions[i2] - std::nearbyint(fractions[i2]));
    });

    for(size_t i = 0; i < (target_ - near_); ++i) {
        ++result_[indices_[i]];
    }

    return result_;
}



namespace esl::economics::markets::walras {
    price_setter::price_setter()
    : price_setter(identity<price_setter>())
    {

    }

    ///
    /// \brief
    ///
    /// \details    Initialises the differentiable variable context to 1.0 times
    /// the initial quotes. In essence, the
    ///             solver starts at 1.0 times the initial quote
    /// \param i
    /// \param traded_assets
    price_setter::price_setter( const identity<price_setter> &i
                              , law::property_map<quote> traded_properties)

    : agent(i)
    , market(i, traded_properties)
    , state(sending_quotes)
    {
        output_clearing_prices_ =
            create_output<std::vector<price>>("clearing_prices");

        this->register_callback<esl::economics::markets::walras::differentiable_order_message>(
                [this](auto msg, esl::simulation::time_interval ti, std::seed_seq &seed) {
                    return ti.upper;
                });
    }

    esl::simulation::time_point
    price_setter::act(esl::simulation::time_interval step, std::seed_seq &seed)
    {
        (void) seed; // TODO
        esl::simulation::time_point next_ = step.upper;
        std::vector<quote> quotes_;

        if(state == sending_quotes) {
            next_ = step.lower;
            for(const auto &[k, v] : traded_properties) {
                (void)k;
                quotes_.push_back(v);
            }
        }else{
            std::unordered_map<
                identity<agent>,
                std::shared_ptr<walras::differentiable_order_message>>
                orders_;

            for(const auto &[k, message_] : inbox) {
                (void)k;
                if(walras::differentiable_order_message::code
                   == message_->type) {
                    auto quote_ = std::dynamic_pointer_cast<
                        walras::differentiable_order_message>(message_);
                    orders_.insert({quote_->sender, quote_});
                }
            }

            if(!orders_.empty()) {
                auto scalars_ = clear_market(orders_, step);
                std::vector<price> prices_;
              
                for(const auto &[k, v] : traded_properties) {
                    (void)v;
                    auto price_ = cash(currencies::USD).value(scalars_[k->identifier]);
                    traded_properties.insert(std::make_pair(k, quote(price_)));  // overwrite
                    prices_.emplace_back(price_);
                    quotes_.emplace_back(quote(price_));    
                }

                output_clearing_prices_->put(step.lower, prices_);
            } else {
                for(const auto &[k, v] : traded_properties) {
                    (void)k;
                    quotes_.push_back(v);  // restore default prices1
                }
            }
        }

        law::property_map<quote> quote_map_;
        {
            size_t sequence_ = 0;
            for(const auto &[k, v] : traded_properties) {
                quote_map_.insert({k, quotes_[sequence_]});
                ++sequence_;
            }
        }

        for(const auto &p : participants) {
            auto m = this->template create_message<
                esl::economics::markets::walras::quote_message>(
                p, step.lower, this->identifier, p, quote_map_);
        }
        state = clearing_market;

        return next_;
    }

    ///
    /// \brief  Clear market using tatonnement. It is assumed the price_setter has received at least one excess demand function from agents.
    ///         
    ///
    std::map<esl::identity<esl::law::property>, double> price_setter::clear_market(
        const std::unordered_map < identity<agent>
                                 , std::shared_ptr<walras::differentiable_order_message>
                                 > &o
            , const esl::simulation::time_interval &step)
    {
        excess_demand_model_context context(
            std::vector<double>(traded_properties.size(), 1.0));

        std::map<esl::identity<esl::law::property>,  quote> quotes_;
        for(const auto &[k, v] : traded_properties) {
            (void)k;
            quotes_.insert({k->identifier, v});
        }

        tatonnement::excess_demand_model model(quotes_);
        interrupt_callback callback_;
        stan::callbacks::logger logger;

        constexpr unsigned int seed  = 0;  // seed
        constexpr unsigned int chain = 0;  // seed

        constexpr double init_radius   = 0;
        constexpr int history_size     = 5;             // 5
        constexpr double init_alpha    = 0.001;      // 0.001
        constexpr double tol_obj       = 1e-13;          // 1e-12
        constexpr double tol_rel_obj   = 1'000;      // 10000
        constexpr double tol_grad      = 1e-9;          // 1e-8
        constexpr double tol_rel_grad  = 1'000;  // 10000000
        constexpr double tol_param     = 1e-9;          // 1e-8
        constexpr int num_iterations   = 10'000;        // 2000
        constexpr bool save_iterations = false;
        constexpr int refresh          = 0;

        state_writer init;
        state_writer parameter;
        init.translation.clear();
        parameter.translation.clear();
        for(auto [k, v]:this->traded_properties) {
            init.translation.push_back(k->identifier);
            parameter.translation.push_back(k->identifier);
        }

        model.excess_demand_functions_.clear();

        for(auto [key, function_] : o) {
            (void)key;
            model.excess_demand_functions_.push_back(function_);
        }

        int return_code = stan::services::optimize::lbfgs(
            model, context, seed, chain, init_radius, history_size, init_alpha,
            tol_obj, tol_rel_obj, tol_grad, tol_rel_grad, tol_param,
            num_iterations, save_iterations, refresh, callback_, logger, init,
            parameter);

        (void)return_code;  // TODO: check return code


        auto clearing_error_ = parameter.errors.back();
        std::cout << "error "<< clearing_error_ << std::endl;


        std::map<esl::identity<esl::law::property>, double> prices_;
        for(auto [k,v]: parameter.states.back()) {
             prices_.insert({k,v});
            
            std::cout << "prices[" << k << "] " << v << std::endl;
        }

        for(auto [key, function_] : o) {
            (void)key;
            model.excess_demand_functions_.push_back(function_);
        }


        std::map<esl::identity<esl::law::property>,
                 std::tuple<esl::economics::quote, double>>
            args;


        for(const auto &[k, v] : o) {
            auto demand_ = v->excess_demand_m(args);
            auto minimum_transfer_ =
                0.0001;  // TODO: this must be deduced from the property...
            // if(fungible){
            //  minimum_transfer_ =
            //}else{
            //  minimum_transfer_ = 1
            //}

            auto property_ = traded_properties.begin();

            for(auto [k2,ed] : demand_) {

                accounting::inventory_filter<law::property> transfers_;
                auto exact_quantity_ = quantity(int(abs(ed)), 1);
                transfers_.insert(property_->first, exact_quantity_);
                property_++;

                if(ed < -minimum_transfer_) {
                    auto transferor_ =
                        dynamic_identity_cast<law::owner<law::property>>(k);
                    auto transferee_ =
                        reinterpret_identity_cast<law::owner<law::property>,
                                                  agent>(*this);




                    auto m =
                        this->template create_message<interaction::transfer>(
                            k, step.lower, (*this), k,
                            transferor_, transferee_, transfers_);


                } else if(ed > minimum_transfer_) {
                    auto transferor_ =
                        reinterpret_identity_cast<law::owner<law::property>,
                                                  agent>(*this);
                    auto transferee_ =
                        dynamic_identity_cast<law::owner<law::property>>(k);
                    auto m =
                        this->template create_message<interaction::transfer>(
                            k, step.lower, (*this), k,
                            transferor_, transferee_, transfers_);
                }
            }
        }
        return prices_;
    }


}  // namespace esl::economics::markets::walras


#include <boost/serialization/export.hpp>

#include <esl/data/serialization.hpp>


BOOST_CLASS_EXPORT(std::vector<esl::economics::price>);
typedef std::tuple<esl::simulation::time_point, std::vector<esl::economics::price>> tuple_time_point_price_vector;
BOOST_CLASS_EXPORT(tuple_time_point_price_vector);
typedef std::vector<std::tuple<esl::simulation::time_point, std::vector<esl::economics::price>>> time_series_price_vector;
BOOST_CLASS_EXPORT(time_series_price_vector);
BOOST_CLASS_EXPORT(esl::data::output<std::vector<esl::economics::price>>);