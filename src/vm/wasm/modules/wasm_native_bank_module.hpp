#pragma once
#include "wasm/wasm_context.hpp"
#include "wasm/wasm_constants.hpp"
#include "wasm/abi_def.hpp"

#include "wasm/exception/exceptions.hpp"
#include "wasm/wasm_log.hpp"
#include "wasm/modules/wasm_router.hpp"
#include "wasm/modules/wasm_native_commons.hpp"
#include "wasm/types/asset.hpp"
#include "wasm/types/regid.hpp"
#include "entities/account.h"
#include "entities/receipt.h"


namespace wasm {

	static uint64_t bank_native_module_id = wasmio_bank;//REGID(0-800);

	class wasm_bank_native_module: public native_module {

		public:
	        wasm_bank_native_module()  {}
	        ~wasm_bank_native_module() {}

	    public:
	    	void register_routes(abi_router& abi_r, action_router& act_r){
	    		abi_r.add_router(bank_native_module_id, abi_handler);
                act_r.add_router(bank_native_module_id, act_handler);
	    	}

	    public:
		  	static void act_handler(wasm_context &context, uint64_t action){
		        switch (action) {
					case N(issue):		//issue a new asset
					     mint(context);
						 return;
					case N(mint):		//mint new asset tokens
					     mint(context);
						 return;
					case N(burn):		//burn asset tokens
					     burn(context);
						 return;
					case N(update):		//update asset profile like owner regid
						 update(context);
						 return;
					 case N(transfer):	//transfer asset tokens
		                 tansfer(context);
		                 return;
		            default:
		                 break;
		        }

		        CHAIN_ASSERT( false,
		                      wasm_chain::action_not_found_exception,
		                      "handler '%s' does not exist in native contract '%s'",
		                      wasm::name(action).to_string(),
		                      wasm::regid(bank_native_module_id).to_string())
	  	     };

		    static std::vector<char> abi_handler() {
		        abi_def abi;

		        if (abi.version.size() == 0) {
		            abi.version = "wasm::abi/1.0";
		        }

				abi.structs.push_back({"issue", "",
					{
						{"symbol",			"symbol"	}, //target asset symbol to issue
						{"owner", 			"regid"		},
						{"name",			"string"	},
						{"total_supply",	"uint64_t"	},
						{"mintable",		"bool"		}
					}
				});
		        abi.structs.push_back({"mint", "",
					{
						{"to", 				"regid"		}, //mint & issue assets to the target holder
						{"quantity", 		"asset" 	}
					}
				});
				abi.structs.push_back({"burn", "",
					{
						{"owner", 			"regid"		}, //only asset owner can burn assets hold by the owner
						{"quantity",		"asset"		}
					}
				});
				abi.structs.push_back({"update", "",
					{
						{"symbol",			"symbol"	}, //target asset symbol to update
						{"owner?", 			"regid"		},
						{"name?",			"string"	},
					}
				});
				abi.structs.push_back({"transfer", "",
					{
						{"from",     		"regid"  	},
						{"to",       		"regid"  	},
						{"quantity", 		"asset"  	},
						{"memo",     		"string" 	}
					}
		        });

				abi.actions.emplace_back( "issue", 		"issue", 	"" );
				abi.actions.emplace_back( "mint", 		"mint", 	"" );
				abi.actions.emplace_back( "burn", 		"burn", 	"" );
				abi.actions.emplace_back( "update", 	"update", 	"" );
		        abi.actions.emplace_back( "transfer", 	"transfer", "" );

		        auto abi_bytes = wasm::pack<wasm::abi_def>(abi);
		        return abi_bytes;
		    }

			static void issue(wasm_context &context) {

				CHAIN_ASSERT(	context._receiver == bank_native_module_id,
							 	wasm_chain::native_contract_assert_exception,
							 	"expect contract '%s', but get '%s'",
							 	wasm::regid(bank_native_module_id).to_string(),
							 	wasm::regid(context._receiver).to_string());

				context.control_trx.run_cost   += context.get_runcost();

		        auto params = wasm::unpack< std::tuple <
												wasm::symbol,
												wasm::regid,
												string,
												uint64_t,
												bool >>(context.trx.data);

				auto symbol				= std::get<0>(params);
		        auto owner              = std::get<1>(params);
		        auto name               = std::get<2>(params);
				auto total_supply		= std::get<3>(params);
				auto mintable			= std::get<4>(params);

				CAsset asset;
				CHAIN_ASSERT( 	!context.database.assetCache.GetAsset(symbol.code().to_string(), asset),
								wasm_chain::asset_type_exception,
								"asset (%s) already issued",
								symbol.to_string() )

				context.require_auth( owner.value );

				CHAIN_ASSERT( 	context.control_trx.GetAccount(context.database, CRegID(owner.value)),
								wasm_chain::account_access_exception,
								"owner account '%s' not exist",
								wasm::regid(owner.value).to_string() )

				asset.asset_symbol	= symbol.code().to_string();
				asset.asset_name	= name;
				asset.asset_type	= AssetType::UIA;
				asset.owner_regid  	= CRegID(owner.value);
				asset.total_supply  = total_supply;
				asset.mintable		= mintable;

				CHAIN_ASSERT( 	context.database.assetCache.SetAsset(asset),
								wasm_chain::level_db_update_fail,
                      			"Update Asset (%s) failure",
                      			symbol.to_string() )
			}

			static void mint(wasm_context &context) {

				CHAIN_ASSERT(	context._receiver == bank_native_module_id,
							 	wasm_chain::native_contract_assert_exception,
							 	"expect contract '%s', but get '%s'",
							 	wasm::regid(bank_native_module_id).to_string(),
							 	wasm::regid(context._receiver).to_string());

				mint_burn_balance(context, true);
			}

			static void burn(wasm_context &context) {

				CHAIN_ASSERT( 	context._receiver == bank_native_module_id,
							  	wasm_chain::native_contract_assert_exception,
							  	"expect contract '%s', but get '%s'",
							  	wasm::regid(bank_native_module_id).to_string(),
							  	wasm::regid(context._receiver).to_string());

				mint_burn_balance(context, false);
			}

			static void update(wasm_context &context) {

		        CHAIN_ASSERT( 	context._receiver == bank_native_module_id,
		                      	wasm_chain::native_contract_assert_exception,
		                      	"expect contract '%s', but get '%s'",
		                      	wasm::regid(bank_native_module_id).to_string(),
		                      	wasm::regid(context._receiver).to_string());

		        context.control_trx.run_cost   += context.get_runcost();

		        auto params = wasm::unpack< std::tuple <
								wasm::symbol,
								std::optional<wasm::regid>,
								std::optional<string> >>(context.trx.data);

				auto symbol							= std::get<0>(params);
		        auto new_owner                      = std::get<1>(params);
		        auto new_name                       = std::get<2>(params);

				CAsset asset;
				CHAIN_ASSERT( 	context.database.assetCache.GetAsset(symbol.code().to_string(), asset),
								wasm_chain::asset_type_exception,
								"asset (%s) not found from d/b",
								symbol.to_string() )

				context.require_auth( asset.owner_regid.GetIntValue() );

				bool to_update = false;

				if (new_owner) {
		        	CHAIN_ASSERT( context.control_trx.GetAccount(context.database, CRegID(new_owner->value)),
								wasm_chain::account_access_exception,
								"new_owner account '%s' does not exist",
								wasm::regid(new_owner->value).to_string() )

					to_update 			= true;
					asset.owner_regid  	= CRegID(new_owner->value);
				}

 				if (new_name) {
					to_update 			= true;
					asset.asset_name	= *new_name;
				}

				CHAIN_ASSERT( 	to_update,
								wasm_chain::native_contract_assert_exception,
                      			"none field found for update")

				CHAIN_ASSERT( 	context.database.assetCache.SetAsset(asset),
								wasm_chain::level_db_update_fail,
                      			"Update Asset (%s) failure",
                      			symbol.to_string() )

			}

		    static void tansfer(wasm_context &context) {

		        CHAIN_ASSERT( 	context._receiver == bank_native_module_id,
		                      	wasm_chain::native_contract_assert_exception,
		                      	"expect contract '%s', but get '%s'",
		                      	wasm::regid(bank_native_module_id).to_string(),
		                      	wasm::regid(context._receiver).to_string());

		        context.control_trx.run_cost   += context.get_runcost();

		        auto transfer_data = wasm::unpack<std::tuple <uint64_t, uint64_t, wasm::asset, string >>(context.trx.data);
		        auto from                        = std::get<0>(transfer_data);
		        auto to                          = std::get<1>(transfer_data);
		        auto quantity                    = std::get<2>(transfer_data);
		        auto memo                        = std::get<3>(transfer_data);

				context.require_auth(from); //from auth

				CHAIN_ASSERT(from != to,             wasm_chain::native_contract_assert_exception, "cannot transfer to self");
		        CHAIN_ASSERT(context.is_account(to), wasm_chain::native_contract_assert_exception, "to account '%s' does not exist", wasm::name(to).to_string() );
		        CHAIN_ASSERT(quantity.is_valid(),    wasm_chain::native_contract_assert_exception, "invalid quantity");
		        CHAIN_ASSERT(quantity.amount > 0,    wasm_chain::native_contract_assert_exception, "must transfer positive quantity");
		        CHAIN_ASSERT(memo.size()  <= 256,    wasm_chain::native_contract_assert_exception, "memo has more than 256 bytes");

				//may not be txAccount since one trx can have multiple signed/authorized transfers (from->to)
				auto spFromAccount = context.control_trx.GetAccount(context.database, CRegID(from));
		        CHAIN_ASSERT( 	spFromAccount,
								wasm_chain::account_access_exception,
								"from account '%s' does not exist",
								wasm::regid(from).to_string())

				auto spToAccount = context.control_trx.GetAccount(context.database, CRegID(to));
		        CHAIN_ASSERT( 	spToAccount,
								wasm_chain::account_access_exception,
								"to account '%s' does not exist",
								wasm::regid(to).to_string())

				CAsset asset;
				string symbol = quantity.symbol.code().to_string();
      			CHAIN_ASSERT( 	context.database.assetCache.GetAsset(symbol, asset),
								wasm_chain::asset_type_exception,
								"asset (%s) not found from d/b",
								symbol )

				transfer_balance( *spFromAccount, *spToAccount, quantity, context );

				WASM_TRACE("transfer from: %s, to: %s, quantity: %s",
							spFromAccount->regid.ToString(), spToAccount->regid.ToString(), quantity.to_string().c_str() )

		        context.notify_recipient(from);
		        context.notify_recipient(to);

		    }
	};
}
