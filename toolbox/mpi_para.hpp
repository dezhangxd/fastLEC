#pragma once

#include <mpi.h>
#include <iostream>
#include <vector>
#include <fstream>
#include <string>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <cstdlib>
#include <algorithm>

extern "C"
{
#include "../deps/kissat/src/kissat.h"
    struct kissat;
}

class CNF
{
public:
    int n_vars, n_clauses;
    std::vector<int> lits;
    std::vector<bool> mask;

    void build_mask(const std::string &cnf_file, int cube_size)
    {

        mask.resize(n_vars + 1, false);

        std::vector<int> cubes;
        size_t last_slash = cnf_file.find_last_of("/\\");
        std::string filename = (last_slash == std::string::npos)
            ? cnf_file
            : cnf_file.substr(last_slash + 1);
        size_t last_dot = filename.find_last_of('.');
        std::string base_name = (last_dot == std::string::npos)
            ? filename
            : filename.substr(0, last_dot);
        std::vector<std::string> parts;
        std::stringstream ss(base_name);
        std::string item;
        while (std::getline(ss, item, '_'))
            parts.push_back(item);
        for (int i = int(parts.size()) - 1; i >= 0; --i)
        {
            std::string &token = parts[i];
            if (!token.empty() &&
                (std::isdigit(token[0]) ||
                 (token[0] == '-' && token.size() > 1 &&
                  std::isdigit(token[1]))))
            {
                bool valid = true;
                for (size_t j = (token[0] == '-' ? 1 : 0); j < token.size();
                     ++j)
                {
                    if (!std::isdigit(token[j]))
                    {
                        valid = false;
                        break;
                    }
                }
                if (valid)
                {
                    cubes.push_back(std::stoi(token));
                }
                else
                {
                    break;
                }
            }
            else
            {
                break;
            }
        }
        std::reverse(cubes.begin(), cubes.end());

        if (cube_size > int(cubes.size()))
        {
            printf("c [build_mask] ERROR: cube_size(%d) > parsed "
                   "cubes.size(%zu)\n",
                   cube_size,
                   cubes.size());
            exit(1);
        }
        if (cube_size > 0)
        {
            cubes = std::vector<int>(cubes.end() - cube_size, cubes.end());
        }
        else
        {
            cubes.clear();
        }

        for (size_t i = 0; i < cubes.size(); ++i)
        {
            mask[abs(cubes[i])] = true;
        }
    }

    void build(const std::string &cnf_file, int cube_size)
    {
        std::ifstream fin(cnf_file);
        std::string line;
        while (getline(fin, line))
        {
            if (line.empty() || line[0] == 'c')
                continue;
            if (line[0] == 'p')
            {
                std::istringstream iss(line);
                std::string tmp_p, tmp_cnf;
                iss >> tmp_p >> tmp_cnf >> n_vars >> n_clauses;
                build_mask(cnf_file, cube_size);
            }
            else
            {
                std::istringstream iss(line);
                int lit;
                while (iss >> lit)
                {
                    lits.push_back(lit);
                }
            }
        }
        fin.close();
    }
};