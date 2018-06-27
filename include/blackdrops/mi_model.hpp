//| Copyright Inria July 2017
//| This project has received funding from the European Research Council (ERC) under
//| the European Union's Horizon 2020 research and innovation programme (grant
//| agreement No 637972) - see http://www.resibots.eu
//|
//| Contributor(s):
//|   - Konstantinos Chatzilygeroudis (konstantinos.chatzilygeroudis@inria.fr)
//|   - Rituraj Kaushik (rituraj.kaushik@inria.fr)
//|   - Roberto Rama (bertoski@gmail.com)
//|
//| This software is the implementation of the Black-DROPS algorithm, which is
//| a model-based policy search algorithm with the following main properties:
//|   - uses Gaussian processes (GPs) to model the dynamics of the robot/system
//|   - takes into account the uncertainty of the dynamical model when
//|                                                      searching for a policy
//|   - is data-efficient or sample-efficient; i.e., it requires very small
//|     interaction time with the system to find a working policy (e.g.,
//|     around 16-20 seconds to learn a policy for the cart-pole swing up task)
//|   - when several cores are available, it can be faster than analytical
//|                                                    approaches (e.g., PILCO)
//|   - it imposes no constraints on the type of the reward function (it can
//|                                                  also be learned from data)
//|   - it imposes no constraints on the type of the policy representation
//|     (any parameterized policy can be used --- e.g., dynamic movement
//|                                              primitives or neural networks)
//|
//| Main repository: http://github.com/resibots/blackdrops
//| Preprint: https://arxiv.org/abs/1703.07261
//|
//| This software is governed by the CeCILL-C license under French law and
//| abiding by the rules of distribution of free software.  You can  use,
//| modify and/ or redistribute the software under the terms of the CeCILL-C
//| license as circulated by CEA, CNRS and INRIA at the following URL
//| "http://www.cecill.info".
//|
//| As a counterpart to the access to the source code and  rights to copy,
//| modify and redistribute granted by the license, users are provided only
//| with a limited warranty  and the software's author,  the holder of the
//| economic rights,  and the successive licensors  have only  limited
//| liability.
//|
//| In this respect, the user's attention is drawn to the risks associated
//| with loading,  using,  modifying and/or developing or reproducing the
//| software by the user in light of its specific status of free software,
//| that may mean  that it is complicated to manipulate,  and  that  also
//| therefore means  that it is reserved for developers  and  experienced
//| professionals having in-depth computer knowledge. Users are therefore
//| encouraged to load and test the software's suitability as regards their
//| requirements in conditions enabling the security of their systems and/or
//| data to be ensured and,  more generally, to use and operate it in the
//| same conditions as regards security.
//|
//| The fact that you are presently reading this means that you have had
//| knowledge of the CeCILL-C license and that you accept its terms.
//|
#ifndef BLACKDROPS_MI_MODEL_HPP
#define BLACKDROPS_MI_MODEL_HPP

#include <Eigen/binary_matrix.hpp>

namespace blackdrops {
    template <typename Params, typename MeanFunction, typename Optimizer>
    class MIModel {
    public:
        MIModel() { _init = false; }

        void learn(const std::vector<std::tuple<Eigen::VectorXd, Eigen::VectorXd, Eigen::VectorXd>>& observations, bool only_limits = false)
        {
            std::vector<Eigen::VectorXd> samples, observs;
            for (size_t i = 0; i < observations.size(); i++) {
                Eigen::VectorXd st, act, pred;
                st = std::get<0>(observations[i]);
                act = std::get<1>(observations[i]);
                pred = std::get<2>(observations[i]);

                Eigen::VectorXd s(st.size() + act.size());
                s.head(st.size()) = st;
                s.tail(act.size()) = act;

                samples.push_back(s);
                observs.push_back(pred);
            }

            _samples = samples;
            _observations = observs;

            if (!_init) {
                _mean = MeanFunction(_samples[0].size());
                _init = true;
            }

            Optimizer optimizer;
            Eigen::VectorXd best_params = optimizer(std::bind(&MIModel::_optimize_model, this, std::placeholders::_1, std::placeholders::_2), _mean.h_params(), true);

            std::cout << "Mean: " << best_params.transpose() << std::endl;

            _mean.set_h_params(best_params);
        }

        void save_data(const std::string& filename) const
        {
            std::ofstream ofs_data(filename);
            for (size_t i = 0; i < _samples.size(); ++i) {
                if (i != 0)
                    ofs_data << std::endl;
                for (size_t j = 0; j < _samples[0].size(); ++j) {
                    ofs_data << _samples[i](j) << " ";
                }
                for (size_t j = 0; j < _observations[0].size(); ++j) {
                    ofs_data << _observations[i](j) << " ";
                }
            }
        }

        std::tuple<Eigen::VectorXd, Eigen::VectorXd> predict(const Eigen::VectorXd& x, bool) const
        {
            Eigen::VectorXd mu = _mean(x, x);
            Eigen::VectorXd ss = Eigen::VectorXd::Zero(mu.size());

            return std::make_tuple(mu, ss);
        }

    protected:
        std::vector<Eigen::VectorXd> _samples, _observations;
        MeanFunction _mean;
        bool _init;

        limbo::opt::eval_t _optimize_model(const Eigen::VectorXd& params, bool eval_grad = false) const
        {
            assert(_samples.size());
            MeanFunction mean(_samples[0].size());
            mean.set_h_params(params);

            double mse = 0.;
            for (size_t i = 0; i < _samples.size(); i++) {
                Eigen::VectorXd mu = mean(_samples[i], _samples[i]);
                Eigen::VectorXd val = _observations[i];

                mse += (mu - val).squaredNorm();
            }

            return limbo::opt::no_grad(-mse);
        }
    };
} // namespace blackdrops

#endif