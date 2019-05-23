//
// Created by zmx on 2019-05-23.
//

#include <eosiolib/eosio.hpp>
#include <eosiolib/asset.hpp>

class token: eosio::contract {
private:
    // @abi table currency
    struct currency {
        eosio::symbol_name key;
        eosio::asset supply;
        eosio::asset max_supply;
        account_name issuer;

        eosio::symbol_name primary_key() const { return key; }
    };

    // @abi table ubalance
    struct ubalance {
        eosio::symbol_name key;
        eosio::asset balance;

        eosio::symbol_name primary_key() const { return key; }
    };

    typedef eosio::multi_index<N(currency), currency> t_currency;
    typedef eosio::multi_index<N(ubalance), ubalance> t_ubalance;

    void validate_asset(eosio::asset asset) {
        auto symbol = asset.symbol;
        eosio_assert(symbol.is_valid(), "invalid symbol");
        eosio_assert(asset.is_valid(), "invalid asset");
        eosio_assert(asset.amount > 0, "asset amount should be above 0");
    }

    void reduce_balance(account_name from, eosio::asset value) {
        t_ubalance tu(_self, from);
        auto symbol = value.symbol;
        auto iter = tu.find(symbol.name());
        eosio_assert(iter != tu.end(), "user doesn't exist");
        eosio_assert(iter->balance >= value, "insufficient");
        tu.modify(iter, from, [&](auto& r) {
            r.balance -= value;
        });
    }

    void add_balance(account_name to, eosio::asset value, account_name ram_payer) {
        t_ubalance tu(_self, to);
        auto symbol = value.symbol;
        auto iter = tu.find(symbol.name());
        if(iter == tu.end()) {
            tu.emplace(ram_payer, [&](auto& r) {
                r.key = symbol.name();
                r.balance = value;
            });
        } else {
            tu.modify(iter, ram_payer, [&](auto& r) {
                r.balance += value;
            });
        }
    }

public:
    token(account_name self): eosio::contract(self) {}

    // @abi action
    void create(account_name issuer, eosio::asset max_supply) {
        require_auth(_self);
        validate_asset(max_supply);
        eosio_assert(is_account(issuer), "to isn't account");

        auto symbol = max_supply.symbol;
        t_currency tc(_self, symbol.name());
        auto iter = tc.find(symbol.name());
        eosio_assert(iter == tc.end(), "token exist");

        tc.emplace(_self, [&](auto& r) {
            r.key = symbol.name();
            r.supply.symbol = symbol;
            r.max_supply = max_supply;
            r.issuer = issuer;
        });
    }

    // @abi action
    void issue(account_name to, eosio::asset value) {
        validate_asset(value);
        eosio_assert(is_account(to), "to isn't account");

        auto symbol = value.symbol;
        t_currency tc(_self, symbol.name());
        auto iter = tc.find(symbol.name());
        eosio_assert(iter != tc.end(), "token doesn't exist");
        require_auth(iter->issuer);
        eosio_assert(value <= iter->max_supply-iter->supply, "supply exceed");

        tc.modify(iter, iter->issuer, [&](auto& r) {
            r.supply += value;
        });

        add_balance(iter->issuer, value, iter->issuer);

        if(to != iter->issuer) {
            SEND_INLINE_ACTION(*this, transfer, {iter->issuer, N(active)}, {iter->issuer, to, value});
        }
    }

    // @abi action
    void transfer(account_name from, account_name to, eosio::asset value) {
        require_auth(from);
        validate_asset(value);
        eosio_assert(from != to, "to can't be from");
        eosio_assert(is_account(to), "to isn't account");
        require_recipient(from);
        require_recipient(to);

        reduce_balance(from, value);
        add_balance(to, value, from);
    }
};

EOSIO_ABI(token, (create)(issue)(transfer))