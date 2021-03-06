#include <functional>
#include <cstdlib>
#include <gsl/gsl_errno.h>
#include <gsl/gsl_math.h>
#include <gsl/gsl_min.h>
#include <gsl/gsl_roots.h>

namespace gsl
{
/// Convert a <c>std::function<double(double)></c> to a value usable with GSL.
/// \param d Input value
/// \param data a std::function<double(double)>
double std_func_to_gsl_function(double d, void *data)
{
    std::function<double(double)> *func = reinterpret_cast<std::function<double(double)>*>(data);
    return (*func)(d);
}

double minimize(const std::function<double(double)> fn,
                double m,
                double a,
                double b,
                const int max_iter,
                const double tolerance,
                const gsl_min_fminimizer_type *min_type)
{
    int iter = 0, status;
    gsl_min_fminimizer *s;
    gsl_function gsl_fn;

    gsl_fn.function = &std_func_to_gsl_function;
    gsl_fn.params = (void*)&fn;

    s = gsl_min_fminimizer_alloc(min_type);
    gsl_min_fminimizer_set(s, &gsl_fn, m, a, b);

    do {
        iter++;
        status = gsl_min_fminimizer_iterate(s);

        m = gsl_min_fminimizer_x_minimum(s);
        a = gsl_min_fminimizer_x_lower(s);
        b = gsl_min_fminimizer_x_upper(s);

        status = gsl_min_test_interval(a, b, tolerance, 0.0);
    } while(status == GSL_CONTINUE && iter < max_iter);
    gsl_min_fminimizer_free(s);
    return m;
}

double find_root(const std::function<double(double)> fn,
                 double a,
                 double b,
                 const int max_iter,
                 const double tolerance,
                 const gsl_root_fsolver_type *solver_type)
{
    int iter = 0, status;
    gsl_root_fsolver *s;
    gsl_function gsl_fn;

    gsl_fn.function = &std_func_to_gsl_function;
    gsl_fn.params = (void*)&fn;

    s = gsl_root_fsolver_alloc(solver_type);
    gsl_root_fsolver_set(s, &gsl_fn, a, b);

    double r = a;

    do {
        iter++;
        status = gsl_root_fsolver_iterate(s);

        r = gsl_root_fsolver_root(s);
        a = gsl_root_fsolver_x_lower(s);
        b = gsl_root_fsolver_x_upper(s);

        status = gsl_root_test_interval(a, b, tolerance, 0.0);
    } while (status == GSL_CONTINUE && iter < max_iter);

    gsl_root_fsolver_free(s);

    return r;
}

} // namespace gsl
