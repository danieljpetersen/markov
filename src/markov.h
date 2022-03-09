#ifndef FI_MARKOV_H
#define FI_MARKOV_H

#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include <random>
#include <algorithm>
#include <chrono>

// todo better rng - rng here is just for standalone example

// my attempt at porting https://github.com/Tw1ddle/markov-namegen-lib - http://www.roguebasin.com/index.php?title=Names_from_a_high_order_Markov_Process_and_a_simplified_Katz_back-off_scheme
// yes, haxe can transpile to c++, but i took a look at the generated output and scratched my head

namespace fi
{
	template <typename T>
	static void vectorInsertUnique(std::vector<T> &Vector, T Element)
	{
		bool Contains = false;
		for (int i = 0; i < Vector.size(); ++i)
		{
			if (Vector[i] == Element)
			{
				Contains = true;
				break;
			}
		}

		if (Contains != true)
		{
			Vector.push_back(Element);
		}
	}

	// ----

	static std::string strRepeat(std::string str, int n)
	{
		std::string Output = std::string("");
		for (int i = 0; i < n; ++i)
		{
			Output += str;
		}
		return Output;
	}

	// ----

	static std::string substrStartEndIndex(std::string str, int StartIndex, int EndIndex)
	{
		int len = EndIndex - StartIndex;
		return str.substr(StartIndex, len);
	}

	// ----

	static std::string strReplaceAll(std::string& str, const std::string& from, const std::string& to)
	{
		size_t start_pos = 0;
		while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
			str.replace(start_pos, from.length(), to);
			start_pos += to.length(); // Handles case where 'to' is a substring of 'from'
		}
		return str;
	}

	// ----

	class Model
	{
	public:
		/**
		 * The order of the model i.e. how many characters this model looks back.
		 */
		unsigned int order;

		/**
		 * Dirichlet prior, like additive smoothing, increases the probability of any item being picked.
		 */
		float prior;

		/**
		 * The alphabet of the training data.
		 */
		std::vector<std::string> alphabet;

		/**
		 * The observations.
		 */
		std::unordered_map<std::string, std::vector<std::string>> observations;

		/**
		 * The Markov chains.
		 */
		std::unordered_map<std::string, std::vector<float>> chains;

	public:
		/**
		 * Creates a new Markov model.
		 * @param   data    The training data for the model, an array of words.
		 * @param   order   The order of model to use, models of order "n" will look back "n" characters within their context when determining the next letter.
		 * @param   prior   The dirichlet prior, an additive smoothing "randomness" factor. Must be in the range 0 to 1.
		 * @param   alphabet    The alphabet of the training data i.e. the set of unique symbols used in the training data.
		 */
		Model(std::vector<std::string> data, unsigned int order, float prior, std::vector<std::string> alphabet)
		{
			if (order < 1) { order = 1; }
			if (prior < 0.0f) { prior = 0.0f; }
			if (prior > 0.1f) { prior = 0.1f; }
			this->order = order;
			this->prior = prior;
			this->alphabet = alphabet;

			train(data);
			buildChains();
		}

		/**
		 * Attempts to generate the next letter in the word given the context (the previous "order" letters).
		 * @param   context The previous "order" letters in the word.
		 */
		std::optional<std::string> generate(std::string& context)
		{
			auto it = chains.find(context);
			if (it == chains.end())
			{
				return std::nullopt;
			}
			else
			{
				return alphabet[selectIndex(&(*it).second)];
			}
		}

		/**
		 * Retrains the model on the newly supplied data, regenerating the Markov chains.
		 * @param   data    The new training data.
		 */
		void retrain(std::vector<std::string> &data)
		{
			train(data);
			buildChains();
		}

		/**
		 * Trains the model on the given training data.
		 * @param   data    The training data.
		 */
		void train(std::vector<std::string> &data)
		{
			while (data.empty() != true)
			{
				std::string d = data.back();
				data.pop_back();
				d = strRepeat("#", order) + d + "#";
				int len = (int)d.size() - order;
				for (int i = 0; i < len; ++i)
				{
					auto key = substrStartEndIndex(d, i, i + order);
					observations[key].push_back(std::string(1, d[i + order]));
				}
			}
		}

		/**
		 * Builds the Markov chains for the model.
		 */
		void buildChains()
		{
			chains.clear();

			for (auto &context : observations)
			{
				std::string key = context.first;
				for (auto &prediction : alphabet)
				{
					auto v = prior + countMatches(context.second, prediction);
					chains[key].push_back(v);
				}
			}
		}

		int countMatches(const std::vector<std::string> &arr, const std::string &v)
		{
			int count = 0;
			for (int i = 0; i < arr.size(); ++i)
			{
				if (arr[i] == v)
				{
					count++;
				}
			}
			return count;
		}

		int selectIndex(const std::vector<float> *chain)
		{
			auto totals = std::vector<float>();
			float accumulator = 0.0f;

			for (int i = 0; i < (*chain).size(); i++)
			{
				accumulator += (*chain).at(i);
				totals.push_back(accumulator);
			}
			float r = ((double) rand() / (RAND_MAX)) * accumulator; // todo better rng this is just for standalone example
			for (int i = 0; i < totals.size(); i++)
			{
				if (r < totals[i])
				{
					return i;
				}
			}

			return 0;
		}
	};

	// ----

	class Generator
	{
	public:
		/**
		* The highest order model used by this generator.
		*
		* Generators own models of order 1 through order "n".
		* Generators of order "n" look back up to "n" characters when choosing the next character.
		*/
		unsigned int order;

		/**
		 * Dirichlet prior, acts as an additive smoothing factor.
		 *
		 * The prior adds a constant probability that a random letter is picked from the alphabet when generating a new letter.
		 */
		float prior;

		/**
		* Whether to fall back to lower orders of models when a higher-order model fails to generate a letter.
		*/
		bool backoff;

		/**
		 * The array of Markov models used by this generator, starting from highest order to lowest order.
		 */
		 std::vector<Model> models;

	public:
		Generator() = default;

		/**
		 * Creates a new procedural word Generator.
		 * @param   data    Training data for the generator, an array of words.
		 * @param   order   Highest order of model to use - models of order 1 through order will be generated.
		 * @param   prior   The dirichlet prior/additive smoothing "randomness" factor.
		 * @param   backoff Whether to fall back to lower order models when the highest order model fails to generate a letter.
		 */
		Generator(std::vector<std::string> data, unsigned int order, float prior, bool backoff)
		{

			for (int i = 0; i < data.size(); i++)
			{
				transform(data[i].begin(), data[i].end(), data[i].begin(), ::tolower);
			}

			this->order = order;
			this->prior = prior;
			this->backoff = backoff;

        	// Identify and sort the alphabet used in the training data
			auto letters = std::vector<std::string>();
			for (auto &word : data)
			{
				for (int i = 0; i < word.size(); i++)
				{
					vectorInsertUnique(letters, std::string(1, word[i]));
				}
			}

			std::sort(letters.begin(), letters.end());

			std::vector<std::string> domain({ "#" });
			domain.insert(domain.end(), letters.begin(), letters.end());

			// create models
			models.clear();
			if (this->backoff)
			{
				for (size_t i = 0; i < order; i++)
				{
					models.push_back(Model(data, order - i, prior, domain)); // From highest to lowest order
				}
			}
			else
			{
				models.push_back(Model(data, order, prior, domain));
			}
		}


		/**
		 * Generates a word.
		 * @return The generated word.
		 */
		std::string generate()
		{
			auto word = strRepeat("#", order);
			auto letter = getLetter(word);
        	while (letter.has_value() && letter.value() != "#")
        	{
       			word = word + letter.value();
			
				letter = getLetter(word);
	        }

    	    return word;
		}


		/**
		 * Generates the next letter in a word.
		 * @param   context The context the models will use for generating the next letter.
		 * @return  The generated letter, or null if no model could generate one.
		 */
		std::optional<std::string> getLetter(std::string &word)
		{
			std::optional<std::string> letter = std::nullopt;
			std::string context = substrStartEndIndex(word, word.size() - order, word.size());
			for (auto &model : models)
			{
				letter = model.generate(context);
				if ((letter.has_value() != true) || (letter.value() == "#"))
				{
					context = context.substr(1);
				}
				else
				{
					break;
				}
			}

			return letter;
		}
	};

	// ----
	/**
	 * An example name generator that builds upon the Generator class. This should be sufficient for most simple name generation scenarios.
	 *
	 * For complex name generators, modifying the Generator class to your specifications may be more appropriate or performant than extending this approach.
	 */
	class NameGenerator
	{
	private:
		/**
		 * The underlying word generator.
		 */
		Generator generator;

	public:

		/**
		 * Creates a new procedural name generator.
		 * @param   data    Training data for the generator, an array of words.
		 * @param   order   Highest order of model to use - models 1 to order will be generated.
		 * @param   prior   The dirichlet prior/additive smoothing "randomness" factor.
		 * @param   backoff Whether to fall back to lower order models when the highest order model fails to generate a letter (defaults to false).
		 */
		NameGenerator(std::vector<std::string> data, int order = 3, float prior = 0.0f, bool backoff = false)
		{
			this->generator = Generator(data, order, prior, backoff);
		}

		/**
		 * Creates a word within the given constraints.
		 * @return  A word that meets the specified constraints, or null if the generated word did not meet the constraints.
		 */
		std::string generateName(int minNameLength, int maxNameLength)
		{
			std::string name;

			int tryCount = 0;
			while (tryCount < 20)
			{
				name = generator.generate();

				strReplaceAll(name, std::string("#"), std::string(""));

				if ((name.size() >= minNameLength) && (name.size() <= maxNameLength))
				{
					break;
				}

				++tryCount;
			}

			return name;
		}
	};
}

#endif