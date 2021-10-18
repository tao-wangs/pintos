typedef int32_t fp;

#define P_BITS 17
#define Q_BITS 14
#define F_MULTI (1 << Q_BITS)

// takes an integer and converts it to fp form
static inline fp i_to_fp (int32_t num){
  return (fp) (num * F_MULTI);
}

//takes numerator and denominator and returns a fraction in fp form
static inline fp create_frac(int32_t top, int32_t bot){
  return (fp) ((top * F_MULTI) / bot);
}

//converts an fp number to int, rounds to nearest integer
static inline int32_t fp_to_i_nearest(fp num){
  if (num >= 0){
    return (int32_t) ((num + (F_MULTI / 2)) / F_MULTI);
  } else {
    return (int32_t) ((num - (F_MULTI / 2)) / F_MULTI);
  }
}

//converts an fp number to int, truncates decimal
static inline int32_t fp_to_i_trunc(fp num){
  return (int32_t) (num / F_MULTI);
}

//add two fp numbers
static inline fp fp_add(fp x, fp y){
  return x + y;
}

//subtract an fp number from another
static inline fp fp_sub(fp x, fp y){
  return x - y;
}

//multiply two fp numbers
static inline fp fp_multi(fp x, fp y){
  return ((int64_t) x) * y / F_MULTI;
}

//divide two fp numbers
static inline fp fp_divide(fp x, fp y){
  return ((int64_t) x) * F_MULTI / y;
}
