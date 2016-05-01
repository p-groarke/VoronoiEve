//
//  EveCrest.cpp
//  EveIndustryEssentials
//
//  Created by Philippe Groarke on 2015-09-06.
//  Copyright (c) 2015 Philippe Groarke All rights reserved.
//

#include "eve_crest.h"
#include "eve_crest_caches.h"

using namespace utility;                    // Common utilities like string conversions
using namespace web;                        // Common features like URIs.
using namespace web::http;                  // Common HTTP functionality
using namespace web::http::client;          // HTTP client features
using namespace concurrency::streams;       // Asynchronous streams

const uri eveMainUri = U("https://crest-tq.eveonline.com");
const std::string eveSendContentType = "application/vnd.ccp.eve.Api-v3+json";

EveCrest::EveCrest()
{
	init();
//	getAllAveragePrices();
//	getAllItemNames();

}

bool EveCrest::is_ready() const
{
	return _is_ready;
}

/***********/
/* Private */
/***********/

pplx::task<json::value> EveCrest::get_values(const web::uri& address)
{
	http_client client(eveMainUri); // Will send the request.

	// Build custom JSON request as per
	// https://developers.eveonline.com/resource/crest
	http_request myReq(methods::GET);
	myReq.headers().set_content_type(eveSendContentType);
	myReq.set_request_uri(address);

	// Start request.
	return client.request(myReq)
	.then([=](http_response response) -> pplx::task<json::value>
	{
		if (response.status_code() != status_codes::OK) {
			std::cout << "Couldn't GET data - "
					<< response.status_code() << std::endl;

			return pplx::task_from_result(json::value());
		}

		return response.extract_json(true);
	});
}


/* Initialization */
void EveCrest::parse_uris(json::object data)
{
	for (auto x : data) {
		if (!x.second.is_object())
			continue;

		std::string name = x.first;
		json::object secObj = x.second.as_object();

		// Sub objects: create custom key, value.
		if (secObj.find(U("href")) == secObj.end()) {
			for (auto y : secObj) {
				if (!y.second.is_object())
					continue;

				std::string k = y.first;
				k[0] = toupper(k[0]);
				std::string v = y.second.as_object()[U("href")].as_string();
				_uri_map[x.first + k] = v;
			}
		} else {
			std::string url = secObj[U("href")].as_string();
			_uri_map[name] = url;
		}
	}

	for (const auto& x : _uri_map) {
		std::cout << x.first << " : " << x.second << std::endl;
	}
}

void EveCrest::init()
{
	get_values(eveMainUri)
	.then([this](pplx::task<json::value> myTask)
	{
		try {
			json::object data = myTask.get().as_object();
			parse_uris(data);
			this->_is_ready.store(true);
			std::cout << "CREST READY" << std::endl;

		} catch (const json::json_exception& e) {
			std::cout << "Couldn't parse JSON - "
			<< e.what() << std::endl;
		}
	});

	// .then([this]()
	// {
	//         // Start tasks in parallel.
	//         std::vector<pplx::task<void>> tasks = {
	//                 pplx::create_task([this] {
	//                         this->get_item_names_and_prices(); })
	//         };

	//         // Wait for all tasks to finish.
	//         pplx::when_all(begin(tasks), end(tasks))
	//         .then([this]() {
	//                 std::cout << "CREST READY" << std::endl;
	//                 this->_is_ready.store(true);
	//         }).wait();
	// });
}

/* Market Prices */
void EveCrest::parse_item_names_and_prices(json::array data)
{
	std::string name = "";
	double averagePrice = 0.0;
	int id = 0;

	for (auto x : data) {
		if (!x.is_object())
			continue;

		json::object obj = x.as_object();

		// Objects {type: {id:"...", name:"..." }}
		if (obj.find("type") != obj.end()) {
			json::object innerObj = obj[U("type")].as_object();

			if (innerObj.find("name") != innerObj.end()) {
				name = innerObj[U("name")].as_string();
			}
			if (innerObj.find("id") != innerObj.end()) {
				id = innerObj[U("id")].as_integer();
			}
		}

		if (obj.find("averagePrice") != obj.end()) {
			averagePrice = obj[U("averagePrice")].as_double();
		}

		std::cout << std::setprecision(2) << std::fixed
		<< name << std::endl
		<< "    " << id << std::endl
		<< "    " << averagePrice << std::endl;

		// Store data.
//                EveItem i;
//                i.name = name;
//                i.typeId = id;
//                i.averagePrice = averagePrice;
//                EveData::getInstance().getItemMap()[name] = i;

                // Keep names (for autocomplete).
//                EveData::getInstance().getNameVector().push_back(name);
	}
	// std::sort(EveData::getInstance().getNameVector().begin(),
	//           EveData::getInstance().getNameVector().end());
}

void EveCrest::get_item_names_and_prices()
{
	get_values(_uri_map["marketPrices"])
	.then([this](pplx::task<json::value> rawData)
	{
		try {
			json::array itemsArray =
					rawData.get().as_object()[U("items")].as_array();

			parse_item_names_and_prices(itemsArray);

		} catch (const json::json_exception& e) {
			std::cout << "Parsing JSON failed - "
					<< e.what() << std::endl;
		}
	}).wait();
}

void EveCrest::get_solar_systems(std::shared_ptr<std::vector<SolarSystem>> out_data)
{
	EveCrestCache::write_solar_system_cache(out_data);
	EveCrestCache::solar_system_cache_is_good();

	return;

	if (!_is_ready) {
		std::cout << "Eve Crest is not ready." << std::endl;
		return;
	}

	get_values(_uri_map["systems"])
	.then([out_data](pplx::task<json::value> rawData)
	{
		try {
			int item_count = rawData.get().as_object()[U("totalCount")].as_integer();
			out_data->reserve(item_count);

			json::array itemsArray =
					rawData.get().as_object()[U("items")].as_array();

			for (const auto& x : itemsArray) {
				json::object obj = x.as_object();

				SolarSystem ss;
				ss.id = obj[U("id")].as_integer();
				ss.id_str = obj[U("id_str")].as_string();
				ss.url = obj[U("href")].as_string();
				ss.name = obj[U("name")].as_string();

				out_data->emplace_back(ss);
			}
		} catch (const json::json_exception& e) {
			std::cout << "Parsing JSON failed - "
					<< e.what() << std::endl;
		}
		return out_data;
	})

	.then([this](std::shared_ptr<std::vector<SolarSystem>> out_data) {
		for (auto& x : *out_data) {
			get_values(x.url)
			.then([&x](pplx::task<json::value> rawData) {
				try {
					json::object obj = rawData.get().as_object();

					json::object sov = obj[U("sovereignty")].as_object();
					x.alliance = sov[U("name")].as_string();
					x.security_class = obj[U("securityClass")].as_string();
					json::object pos = obj[U("position")].as_object();
					x.y = pos[U("y")].as_double();
					x.x = pos[U("x")].as_double();
					x.z = pos[U("z")].as_double();

					std::cout << x;
				} catch (const json::json_exception& e) {
					std::cout << "Parsing JSON failed - "
							<< e.what() << std::endl;
				}
			}).wait();
		}
		return out_data;
	})

	.then([](std::shared_ptr<std::vector<SolarSystem>> out_data) {
		std::cout << "DONE" << std::endl;
//		for (const auto& x : *out_data) {
//			std::cout << x;
//		}
	});
}


//std::vector<pplx::task<void>> tasks;
//
//for (auto& x : *out_data) {
//	tasks.emplace_back(
//		pplx::create_task([this, &x] {
//			get_values(x.url)
//			.then([&x](pplx::task<json::value> rawData) {
//				try {
//					json::object obj = rawData.get().as_object();
//
//					json::object sov = obj[U("sovereignty")].as_object();
//					x.alliance = sov[U("name")].as_string();
//					x.security_class = obj[U("securityClass")].as_string();
//					json::object pos = obj[U("position")].as_object();
//					x.y = pos[U("y")].as_double();
//					x.x = pos[U("x")].as_double();
//					x.z = pos[U("z")].as_double();
//
//					std::cout << x;
//				} catch (const json::json_exception& e) {
//					std::cout << "Parsing JSON failed - "
//							<< e.what() << std::endl;
//				}
//			});
//		})
//	);
//}
//
//// Wait for all tasks to finish.
//pplx::when_all(begin(tasks), end(tasks))
//.then([=]() {
//	for (const auto& x : *out_data) {
//		std::cout << x;
//	}
//}).wait();

// concurrency::task_group task_group;
// for (const auto& x : tasks) {
// 	task_group.run(x);
// }
// task_group.wait();
