#include "lcfit2_nlopt.h"

#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdio.h>

#include <nlopt.h>

#include "lcfit2.h"

const size_t MAX_ITERATIONS = 1000;

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

double lcfit2_opt_fdf_nlopt(unsigned p, const double* x, double* grad, void* data)
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
        // normalized log-likelihood error
        const double err = lnl[i] - (lcfit2_lnl(t[i], &model) - lcfit2_lnl(model.t0, &model));

        sum_sq_err += w[i] * pow(err, 2.0);

        if (grad) {
            lcfit2_gradient(t[i], &model, grad_i);

            grad[0] -= 2 * w[i] * err * grad_i[0];
            grad[1] -= 2 * w[i] * err * grad_i[1];
        }
    }

#ifdef LCFIT2_VERBOSE
    lcfit2_print_state_nlopt(sum_sq_err, x, grad);
#endif

    return sum_sq_err;
}

//
// nlopt expects constraints of the form fc(x) <= 0
//

// constraint: c > m
// becomes m - c < 0

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

// constraint: c + m - v > 0 which implies t0 <= (1/r)*log((c+m)/(c-m))
// becomes t0 - (1/r)*log((c+m)/(c-m)) <= 0
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


int lcfit2_fit_weighted_nlopt(const size_t n, const double* t, const double* lnl,
                              const double* w, lcfit2_bsm_t* model)
{
    lcfit2_fit_data data = { n, t, lnl, w, model->t0, model->d1, model->d2 };

    const double lower_bounds[2] = { 1.0, 1.0 };
    const double upper_bounds[2] = { INFINITY, INFINITY };

    nlopt_opt opt = nlopt_create(NLOPT_LD_SLSQP, 2);
    nlopt_set_min_objective(opt, lcfit2_opt_fdf_nlopt, &data);
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
