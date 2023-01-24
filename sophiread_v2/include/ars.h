#include "clustering.h"

/**
 * @brief Class for the AdaptiveRadiusSearch (ARS) algorithm
 *
 */
class ARS : public ClusteringAlgorithm {
 public:
  ARS(double minRadius, double maxRadius, double radiusStep)
      : minRadius_(minRadius), maxRadius_(maxRadius), radiusStep_(radiusStep){};
  void initialize();
  void fit(const std::vector<std::vector<double>>& data);
  std::vector<int> predict(const std::vector<std::vector<double>>& data);

 private:
  double minRadius_;   // The minimum radius
  double maxRadius_;   // The maximum radius
  double radiusStep_;  // The step size for increasing the radius
};
