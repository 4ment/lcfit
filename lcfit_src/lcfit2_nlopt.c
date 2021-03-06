/**
 * \file lcfit2_nlopt.c
 * \brief Implementation of lcfit2 optimization using NLopt.
 */

#include "lcfit2_nlopt.h"

#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdio.h>

#include <nlopt.h>

#include "lcfit2.h"

static const size_t MAX_ITERATIONS = 1000;

void lcfit2_print_state_nlopt(double sum_sq_err, const double* x, const double* grad)
{
    size_t iter = 0;

    fprintf(stderr, "N[%4zu] rsse = %.3f", iter, sqrt(sum_sq_err));
    fprintf(stderr, ", model = { %.3f, %.3f }",
            x[0], x[1]);
    if (grad) {
        fprintf(stderr, ", grad = { %.6f, %.6f }",
                grad[0], grad[1]);
    }
    fprintf(stderr, "\n");
}

/** NLopt objective function and its gradient.
 *
 * This function expects that the observed log-likelihoods have been
 * normalized such that the log-likelihood at \f$t_0\f$ is zero.
 *
 * \param[in]  p     Number of model parameters.
 * \param[in]  x     Model parameters to evaluate.
 * \param[out] grad  Gradient of the objective function at \c x.
 * \param[in]  data  Observed log-likelihood data to fit.
 *
 * \return Sum of squared error from observed log-likelihoods.
 */
double lcfit2n_opt_fdf_nlopt(unsigned p, const double* x, double* grad, void* data)
{
    lcfit2_fit_data* d = (lcfit2_fit_data*) data;

    const size_t n = d->n;
    const double* t = d->t;
    const double* lnl = d->lnl;
    const double* w = d->w;

    lcfit2_bsm_t model = { x[0], x[1], d->t0, d->d1, d->d2 };

    double sum_sq_err = 0.0;

    if (grad) {
        grad[0] = 0.0;
        grad[1] = 0.0;
    }

    double grad_i[2];

    for (size_t i = 0; i < n; ++i) {
        //
        // We expect that the observed log-likelihoods have already
        // been normalized. The error is therefore the sum of squared
        // differences between those log-likelihoods and the
        // normalized lcfit2 log-likelihoods f(t[i]) - f(t0).
        //

        const double err = lnl[i] - lcfit2_norm_lnl(t[i], &model);

        sum_sq_err += w[i] * pow(err, 2.0);

        if (grad) {
            lcfit2n_gradient(t[i], &model, grad_i);

            grad[0] -= 2 * w[i] * err * grad_i[0];
            grad[1] -= 2 * w[i] * err * grad_i[1];
        }
    }

#ifdef LCFIT2_VERBOSE
    lcfit2_print_state_nlopt(sum_sq_err, x, grad);
#endif /* LCFIT2_VERBOSE */

    return sum_sq_err;
}

/** NLopt constraint function and its gradient for enforcing that \f$c > m\f$.
 *
 * NLopt expects constraint functions of the form \f$f_c(x) \leq 0\f$,
 * so we use \f$f_c(x) = m - c\f$. That \f$c\f$ must be strictly
 * greater than \f$m\f$ is handled by the SLSQP algorithm itself, as
 * the lcfit2 log-likelihood function will return \c NaN in the case
 * where \f$c = m\f$.
 *
 * \param[in]  p     Number of model parameters.
 * \param[in]  x     Model parameters to evaluate.
 * \param[out] grad  Gradient of the constraint function at \c x.
 * \param[in]  data  Observed log-likelihood data (unused).
 *
 * \return Value of the constraint function at \c x.
 */
double lcfit2_cons_cm_nlopt(unsigned p, const double* x, double* grad, void* data)
{
    const double c = x[0];
    const double m = x[1];

    if (grad) {
        grad[0] = -1.0;
        grad[1] = 1.0;
    }

    return m - c;
}

/** NLopt constraint function and its gradient for enforcing that \f$c + m - \nu > 0\f$.
 *
 * The constraint \f$c + m - \nu > 0\f$ implies that
 * \f[
 *   t_0 \leq \frac{1}{r} \log \left( \frac{c + m}{c - m} \right).
 * \f]
 * NLopt expects constraint functions of the form \f$f_c(x) \leq 0\f$, so we use
 * \f[
 *   f_c(x) = t_0 - \frac{1}{r} \log \left( \frac{c + m}{c - m} \right).
 * \f]
 *
 * \param[in]  p     Number of model parameters.
 * \param[in]  x     Model parameters to evaluate.
 * \param[out] grad  Gradient of the constraint function at \c x.
 * \param[in]  data  Observed log-likelihood data.
 *
 * \return Value of the constraint function at \c x.
 */
double lcfit2_cons_cmv_nlopt(unsigned p, const double* x, double* grad, void* data)
{
    lcfit2_fit_data* d = (lcfit2_fit_data*) data;

    const double c = x[0];
    const double m = x[1];
    const double t_0 = d->t0;
    const double f_2 = d->d2;

    if (grad) {
        grad[0] = (1.0L/2.0L)*pow(c - m, 2)*(-1/(c - m) + (c + m)/pow(c - m, 2))/(sqrt(-c*f_2*m/(c + m))*(c + m)) - 1.0L/2.0L*log((c + m)/(c - m))/sqrt(-c*f_2*m/(c + m)) - 1.0L/4.0L*(c - m)*(-c*f_2*m/pow(c + m, 2) + f_2*m/(c + m))*log((c + m)/(c - m))/pow(-c*f_2*m/(c + m), 3.0L/2.0L);

        grad[1] = -1.0L/2.0L*pow(c - m, 2)*(1.0/(c - m) + (c + m)/pow(c - m, 2))/(sqrt(-c*f_2*m/(c + m))*(c + m)) + (1.0L/2.0L)*log((c + m)/(c - m))/sqrt(-c*f_2*m/(c + m)) - 1.0L/4.0L*(c - m)*(-c*f_2*m/pow(c + m, 2) + c*f_2/(c + m))*log((c + m)/(c - m))/pow(-c*f_2*m/(c + m), 3.0L/2.0L);
    }

    return t_0 - 1.0L/2.0L*(c - m)*log((c + m)/(c - m))/sqrt(-c*f_2*m/(c + m));
}

int lcfit2n_fit_weighted_nlopt(const size_t n, const double* t, const double* lnl,
                               const double* w, lcfit2_bsm_t* model)
{
    lcfit2_fit_data data = { n, t, lnl, w, model->t0, model->d1, model->d2 };

    const double lower_bounds[2] = { 1.0, 1.0 };
    const double upper_bounds[2] = { INFINITY, INFINITY };

    nlopt_opt opt = nlopt_create(NLOPT_LD_SLSQP, 2);
    nlopt_set_min_objective(opt, lcfit2n_opt_fdf_nlopt, &data);
    nlopt_set_lower_bounds(opt, lower_bounds);
    nlopt_set_upper_bounds(opt, upper_bounds);

    nlopt_add_inequality_constraint(opt, lcfit2_cons_cm_nlopt, &data, 0.0);
    nlopt_add_inequality_constraint(opt, lcfit2_cons_cmv_nlopt, &data, 0.0);

    nlopt_set_xtol_rel(opt, sqrt(DBL_EPSILON));
    nlopt_set_maxeval(opt, MAX_ITERATIONS);

    double x[2] = { model->c, model->m };
    double sum_sq_err = 0.0;

    int status = nlopt_optimize(opt, x, &sum_sq_err);

    model->c = x[0];
    model->m = x[1];

    nlopt_destroy(opt);
    return status;
}
