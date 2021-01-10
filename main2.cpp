#include "Auth.h"
#include "API.h"
#include <iostream>
#include <vector>
#include <math.h>
#include <stdlib.h>
#include <string>
#include <chrono>
#include <thread>


const int STACK_DEPTH = 5;
const int TRANSACTION_FREQ = 3;
const double MIN_PROFIT = 0.00;

double possible_gain(double size_in_crypto, double current_price);

int main()
{

	// SANDBOX CREDENTIALS
	std::string api_key = "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX";
	std::string secret = "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX";
	std::string passcode = "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX";
	std::string uri = "https://api-public.sandbox.pro.coinbase.com";
	
	std::string product_id = "BTC-GBP";
	std::string crypto = "BTC";
	std::string currency = "GBP";

	Auth auth(api_key, secret, passcode);
	API api;
	api.uri = uri;
	api.product_id = product_id;
	api.auth = auth;

	double balance;
	double chunk_size;
	std::vector<transaction> transaction_stack;
	transaction current_transaction;
	double buy_price;
	double BTC_to_transact;
	double target_value;
	std::string returned;
	int try_counter;
	
	while (true)
	{
		if (transaction_stack.empty())
		{
			// If there are no transactions in the stack, reset the size of each transaction
			balance = api.Get_Balance(currency);
			balance = (balance / STACK_DEPTH) * (STACK_DEPTH - 1);
			chunk_size = floorf((balance / STACK_DEPTH) * 100) / 100;

			// Make an initial transaction
			// TODO - Account for spread. Or maybe don't?
			buy_price = std::stod(api.Get_Buy_Price());
			BTC_to_transact = chunk_size / buy_price;
			
			// Make initial transaction
			returned = api.Place_Limit_Order("buy", std::to_string(buy_price), std::to_string(BTC_to_transact));
			
			// Ensure transaction is fully complete before proceeding
			current_transaction = api.Get_Order(returned);
			current_transaction.buy_price = buy_price;
			
			try_counter = 0;
			// Check of order has fully completed. If not completed within 3 mins, delete order
			while ((not current_transaction.settled) && (try_counter < 36))
			{
				++try_counter;
				std::cout << "." << std::endl;
				std::this_thread::sleep_for(std::chrono::seconds(5));
				current_transaction = api.Get_Order(returned);
				current_transaction.buy_price = buy_price;
			}
			

			if (not current_transaction.settled)
			{
				if (api.Delete_Order(returned)) 
				{
					// Ouput deleted order message
					std::cout << "TRANSACTION CANCELLED. Order not filled in time." << std::endl;
				}
				else
				{
					// if order is not fully complete and cannot be deleted, output message and stop running
					std::cout << "TRANSACTION INCOMPLETE BUT CANNOT BE CANCELLED. Exiting program. Please check status of orders on exchange and resolve before resuming trading." << std::endl;
					return 1;
				}
			}
			else
			{
				// Add transaction to stack
				transaction_stack.push_back(current_transaction);

				// Ticker output
				balance = api.Get_Balance(currency);

				std::cout << "Current buy price: " << buy_price << std::endl;
				std::cout << "Your balance: " << balance << " " << currency << std::endl;
				std::cout << "Bought: " << current_transaction.actual_size << crypto << " for " << current_transaction.true_cost << currency << std::endl << std::endl;
			}
			
			
		}
		else
		{	
			// Check whether to sell last transaction in the stack
			current_transaction = transaction_stack.back();
			buy_price = std::stod(api.Get_Buy_Price());
			BTC_to_transact = std::stod(current_transaction.actual_size);
			target_value = std::stod(current_transaction.true_cost);
		
			if ((possible_gain(BTC_to_transact, buy_price) + MIN_PROFIT) > target_value)
			{
				// Sell last transaction
				returned = api.Place_Limit_Order("sell", std::to_string(buy_price), std::to_string(BTC_to_transact));

				// Ensure transaction is fully complete before proceeding
				current_transaction = api.Get_Order(returned);

				try_counter = 0;
				// check of order has fully completed. if not completed within 3 mins, delete order
				while ((not current_transaction.settled) && (try_counter < 36))
				{
					++try_counter;
					std::cout << "." << std::endl;
					std::this_thread::sleep_for(std::chrono::seconds(5));
					current_transaction = api.Get_Order(returned);
				}
				
				if (not current_transaction.settled)
				{
					if (api.Delete_Order(returned)) 
					{
						// Ouput deleted order message
						std::cout << "TRANSACTION CANCELLED. Order not filled in time." << std::endl;
					}
					else
					{
						// if order is not fully complete and cannot be deleted, output message and stop running
						std::cout << "TRANSACTION INCOMPLETE BUT CANNOT BE CANCELLED. Exiting program. Please check status of orders on exchange and resolve before resuming trading." << std::endl;
						return 1;
					}
				}
				else
				{
					// Remove transaction from stack
					transaction_stack.pop_back();
				
					// Ticker output
					balance = api.Get_Balance(currency);
					std::cout << "Current buy price: " << buy_price << std::endl;
					std::cout << "Your balance: " << balance << " " << currency << std::endl;
					std::cout << "Sold: " << current_transaction.actual_size << crypto << " for " << current_transaction.true_cost << currency << std::endl << std::endl;
				
				}
			}
			else if (transaction_stack.size() == STACK_DEPTH)
			{
				// Condition where transaction would not profit, but stack depth has been reached
				// Ticker output

				balance = api.Get_Balance(currency);

				std::cout << "NO TRANSACTION!" << std::endl;
				std::cout << "Your balance: " << balance << " " << currency << std::endl;
				std::cout << "Likely return: " << (possible_gain(BTC_to_transact, buy_price) + MIN_PROFIT) <<std::endl;
				std::cout << "Required return: " << target_value << std::endl << std::endl;
			}	
			else
			{
				// Check whether last transaction was at lower buy price than current market
				current_transaction = transaction_stack.back();
				buy_price = std::stod(api.Get_Buy_Price());

				if (current_transaction.buy_price < buy_price)
				{
					// Current price is worse than last transaction
					std::cout << "NO TRANSACTION!" << std::endl;
					std::cout << "Your balance: " << balance << " " << currency << std::endl;
					std::cout << "Current Price: " << buy_price <<std::endl;
					std::cout << "Last bought at: " << current_transaction.buy_price << std::endl << std::endl;
				}
				else
				{
					//  Stack depth not reached and current price is lower than last transaction, so buy
					returned = api.Place_Limit_Order("buy", std::to_string(buy_price), std::to_string(BTC_to_transact));
			
					// Ensure transaction is fully complete before proceeding
					current_transaction = api.Get_Order(returned);
					current_transaction.buy_price = buy_price;

					try_counter = 0;
					// check of order has fully completed. if not completed within 3 mins, delete order
					while ((not current_transaction.settled) && (try_counter < 36))
					{
						++try_counter;
						std::cout << "." << std::endl;
						std::this_thread::sleep_for(std::chrono::seconds(4));
						current_transaction = api.Get_Order(returned);
						current_transaction.buy_price = buy_price;
					}
			

					if (not current_transaction.settled)
					{
						if (api.Delete_Order(returned))
						{
							// Ouput deleted order message
							std::cout << "TRANSACTION CANCELLED. Order not filled in time." << std::endl;
						}
						else
						{
							// if order is not fully complete and cannot be deleted, output message and stop running
							std::cout << "TRANSACTION INCOMPLETE BUT CANNOT BE CANCELLED. Exiting program. Please check status of orders on exchange and resolve before resuming trading." << std::endl;
							return 1;
						}
					}
					else
					{
					// Add transaction to stack
					transaction_stack.push_back(current_transaction);

					// Ticker output
					balance = api.Get_Balance(currency);
					std::cout << "Current buy price: " << buy_price << std::endl;
					std::cout << "Your balance: " << balance << " " << currency << std::endl;
					std::cout << "Bought: " << current_transaction.actual_size << crypto << " for " << current_transaction.true_cost << currency << std::endl << std::endl;
					}
				}
			}
		}
		// Wait before attempting next transaction
		std::this_thread::sleep_for(std::chrono::seconds(TRANSACTION_FREQ));
	}

	return 0;
}


double possible_gain(double size_in_crypto, double current_price)
{
	return (size_in_crypto * current_price) * 0.995;
}
