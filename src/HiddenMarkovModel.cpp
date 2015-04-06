#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include "HiddenMarkovModel.hpp"

using namespace std;


/*
 * Utility Functions
 */

template <typename T>
vector<T> split(const string& line)
{
	vector<T> ret;
	string::const_iterator i = line.begin(), j, end = line.end();

	while (i != end && j != end)
	{
		while (isspace(*i) && i != end) ++i;
		j = i;
		while (!isspace(*j) && j != end) ++j;

		if (i != j)
			ret.push_back(string(i, j));
		i = j;
	}
	return ret;
}

/* Template specializations must be defined before the first use of that specialization.
 * C++ templates. Gah. */
template <>
vector<int> split(const string& line)
{
	vector<int> ret;
	string::const_iterator i = line.begin(), j, end = line.end();

	while (i != end && j != end)
	{
		while (isspace(*i) && i != end) ++i;
		j = i;
		while (!isspace(*j) && j != end) ++j;

		if (i != j)
		{
			string str(i, j);
			ret.push_back(strtol(str.c_str(), NULL, 10));
		}
		i = j;
	}
	return ret;
}

template <>
vector<double> split(const string& line)
{
	vector<double> ret;
	string::const_iterator i = line.begin(), j, end = line.end();

	while (i != end && j != end)
	{
		while (isspace(*i) && i != end) ++i;
		j = i;
		while (!isspace(*j) && j != end) ++j;

		if (i != j)
		{
			string str(i, j);
			ret.push_back(strtod(str.c_str(), NULL));
		}
		i = j;
	}
	return ret;
}


/* Return a vector of observation sequences from a .obs file. */
vector<vector<string> > parseObsFile(const string& filename)
{
	ifstream file(filename);
	if (!file.is_open())
		throw runtime_error("file not found: " + string(filename));

	int count;
	file >> count;
	file.ignore(numeric_limits<streamsize>::max(), '\n');

	/* Vector of observation sequences. */
	vector<vector<string> > observations(count);

	for (int i = 0; i < count; ++i)
	{
		file.ignore(numeric_limits<streamsize>::max(), '\n');

		string line;
		getline(file, line);

		observations[i] = split<string>(line);
	}
	return observations;
}

/*
 * HiddenMarkovModel class definitions
 */

HiddenMarkovModel::HiddenMarkovModel(const string& filename)
{
	ifstream file(filename);
	if (!file.is_open())
		throw runtime_error("file not found: " + string(filename));

	/* Create HMM based on input file, which is formatted like so:
	 *
	 * The first line contains integers N (number of states), M (number of observation symbols),
	 * and T (number of time steps or length of oberservation sequences). 
	 *
	 * The second contains each individual HMM state.
	 *
	 * The third line contains each possible observation symbol. */
	string line;

	getline(file, line);
	vector<int> sizes = split<int>(line);

	// initialize number of states and observation symbols
	_numOfStates = sizes[0];
	_numOfOutputs = sizes[1];

	// get state names
	getline(file, line);
	vector<string> stateNames = split<string>(line);
	// initialize all state names
	_stateNames = stateNames;

	// get output names
	getline(file, line);
	vector<string> outputNames = split<string>(line);

	// consume "a:"
	file.ignore(numeric_limits<streamsize>::max(), '\n');

	// initialize state transition probability matrix
	for (auto i = stateNames.begin(); i != stateNames.end(); ++i)
	{
		getline(file, line);
		vector<double> curLine = split<double>(line);
		int col = 0;

		for (auto j = stateNames.begin(); j != stateNames.end(); ++j)
			_transitions[*i][*j] = curLine[col++];
	}

	// consume "b:"
	file.ignore(numeric_limits<streamsize>::max(), '\n');

	// initialize output emission probability matrix
	for (auto i = stateNames.begin(); i != stateNames.end(); ++i)
	{
		getline(file, line);
		vector<double> curLine = split<double>(line);
		int col = 0;

		for (auto j = outputNames.begin(); j != outputNames.end(); ++j)
			_emissions[*i][*j] = curLine[col++];
	}

	// consume "pi:"
	file.ignore(numeric_limits<streamsize>::max(), '\n');

	// set initial state probabilties
	getline(file, line);
	vector<double> tmp = split<double>(line);
	int col = 0;
	for (auto i = stateNames.begin(); i != stateNames.end(); ++i)
	{
		_initStates[*i] = tmp[col++];
	}
}


double HiddenMarkovModel::transition(const std::string& stt1, const std::string& stt2)
{
	// check if this state name exists as a key in our map
	if (_transitions.find(stt1) == _transitions.end())
		throw runtime_error("No such state: " + stt1);
	if (_transitions[stt1].find(stt2) == _transitions[stt1].end())
		throw runtime_error("No such state: " + stt2);

	return _transitions[stt1][stt2];
}


double HiddenMarkovModel::emission(const std::string& stt, const std::string& out)
{
	if (_emissions.find(stt) == _emissions.end())
		throw runtime_error("No such state: " + stt);
	if (_emissions[stt].find(out) == _emissions[stt].end())
		throw runtime_error("No such output: " + out);

	return _emissions[stt][out];
}


double HiddenMarkovModel::initState(const std::string& stt)
{
	if (_emissions.find(stt) == _emissions.end())
		throw runtime_error("No such state: " + stt);

	return _initStates[stt];
}


double HiddenMarkovModel::initEval(const string& out, const string& stt)
{
	return initState(stt) * emission(stt, out);
}


double HiddenMarkovModel::eval(const string& out, const string stts[2])
{
	return transition(stts[0], stts[1]) * emission(stts[1], out);
}


double HiddenMarkovModel::eval(const vector<string>& out, const vector<string>& stt)
{
	if (out.size() != stt.size())
		return 0;

	vector<string>::const_iterator curStt = stt.begin(), curOut = out.begin();
	double ret = initEval(*curOut++, *curStt);

	while (curOut != out.end())
	{
		string tmp[2] = {*curStt, *(++curStt)};
		ret *= eval(*curOut++, tmp);
	}

	return ret;
}


/* Treat t as the time marker at each point in the observation sequence. */
double HiddenMarkovModel::forwardHelper(const vector<string>& obs, int t, const string& curStt)
{
	/* Base case: no previous paths, so the current state must be the initial state. */
	if (t == 0)
		return initEval(obs[t], curStt);

	double sum = 0;

	/* Sum up probabilities of all paths leading to curStt. */
	for (auto stt : _stateNames)
		sum += forwardHelper(obs, t-1, stt) * transition(stt, curStt);

	return sum * emission(curStt, obs[t]);
}

vector<double> HiddenMarkovModel::forward(const string& filename)
{
	/* Vector of observation sequences. */
	vector<vector<string> > observations = parseObsFile(filename);
	vector<double> ret;

	/* Iterate through each sequence of observations. */
	for (auto obs : observations)
	{
		double sum = 0;

		for (auto stt : _stateNames)
			sum += forwardHelper(obs, obs.size()-1, stt);

		ret.push_back(sum);
	}

	return ret;
}


double HiddenMarkovModel::backwardHelper(const vector<string>& obs, int t, const string& curStt)
{
	/* Base case: no next paths, so the current state must be the final state. */
	if (t == static_cast<int>(obs.size()-1))
		return 1;

	double sum = 0;

	/* Sum up probabilities of all paths out from curStt. */
	for (auto stt : _stateNames)
		sum += transition(curStt, stt) * emission(stt, obs[t+1]) * backwardHelper(obs, t+1, stt);

	return sum;
}

vector<double> HiddenMarkovModel::backward(const string& filename)
{
	/* Vector of observation sequences. */
	vector<vector<string> > observations = parseObsFile(filename);
	vector<double> ret;

	/* Iterate through each sequence of observations. */
	for (auto obs : observations)
	{
		double sum = 0;

		for (auto stt : _stateNames)
			sum += initState(stt) * emission(stt, obs[0]) * backwardHelper(obs, 0, stt);

		ret.push_back(sum);
	}

	return ret;
}


// TODO: how to recreate a path???
double HiddenMarkovModel::viterbiHelper(const vector<string>& obs, vector<string>& path,
										int t, const string& curStt)
{
	if (t == 0)
		return initEval(obs[t], curStt);

	double maxProb = 0;
	string maxStt;

	for (auto stt : _stateNames)
	{
		double curr = viterbiHelper(obs, path, t-1, stt) * transition(stt, curStt);
		
		if (curr > maxProb)
		{
			maxProb = curr;
			maxStt = stt;
		}
	}

	if (!maxStt.empty())
		path.push_back(maxStt);

	return maxProb * emission(curStt, obs[t]);
}


std::vector<std::vector<std::string> > HiddenMarkovModel::viterbi(const std::string& filename)
{
	vector<vector<string> > observations = parseObsFile(filename);
	vector<vector<string> > bestPaths;

	/* Iterate through each sequence of observations. */
	for (auto obs : observations)
	{
		/* Current state path for this observation. */
		vector<string> curPath;

		for (auto stt : _stateNames)
		{
			viterbiHelper(obs, curPath, obs.size()-1, stt);
		}

		bestPaths.push_back(curPath);
	}

	return bestPaths;
}
