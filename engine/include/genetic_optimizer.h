#pragma once
#include <vector>
#include <memory>
#include "ast.h"
#include "execution_plan.h"

namespace sqlopt {

class GeneticOptimizer {
public:
    GeneticOptimizer() = default;

    // Optimize using genetic algorithm
    ExecutionPlan optimize(const SelectQuery& query);

private:
    // Genetic algorithm components
    struct Individual {
        ExecutionPlan plan;
        double fitness;
    };

    // Population management
    std::vector<Individual> population_;
    size_t population_size_ = 50;
    size_t generations_ = 100;

    // Genetic operators
    Individual crossover(const Individual& parent1, const Individual& parent2);
    void mutate(Individual& individual);
    double calculateFitness(const ExecutionPlan& plan);

    // Selection
    Individual tournamentSelection();
};

} // namespace sqlopt
