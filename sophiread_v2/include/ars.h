#include "clustering.h"
#include "peakfitting.h"

/**
 * @brief Class for the AdaptiveRadiusSearch (ARS) algorithm
 *
 */
class ARS : public ClusteringAlgorithm {
 public:
  ARS(double minRadius, double maxRadius, double radiusStep)
      : minRadius_(minRadius), maxRadius_(maxRadius), radiusStep_(radiusStep){};
  void initialize(std::string method = "centroid");
  void fit(const std::vector<Hit>& data);
  std::vector<NeutronEvent> get_events(const std::vector<Hit>& data);
  ~ARS() = default;

 private:
  double minRadius_;                // The minimum radius
  double maxRadius_;                // The maximum radius
  double radiusStep_;               // The step size for increasing the radius
  std::vector<int> clusterLabels_;  // The cluster labels for each hit
  ClusteringAlgorithm* peakFittingAlgorithm_;  // The clustering algorithm
};
