
float mandelbrot(vec2 c) {
  const int FRACTAL_ITERS = 10000;
  int i = 0;
  vec2 zn = c;
  float magSq;
  for (; i < FRACTAL_ITERS; ++i) {
    zn = vec2(zn.x * zn.x - zn.y * zn.y + c.x, 2.0 * zn.x * zn.y + c.y);
    magSq = dot(zn, zn);
    if (magSq > 4.0) {
      break;
    }
  }

  float mag = sqrt(magSq);

  return (float(i + 1) - log(log2(mag))) / float(FRACTAL_ITERS);
  // return float(i) / float(FRACTAL_ITERS - 1);
}

vec2 gradMandelbrot(vec2 c, float h) {
  float r = mandelbrot(c + vec2(h, 0.0));
  float l = mandelbrot(c + vec2(-h, 0.0));
  float u = mandelbrot(c + vec2(0.0, h));
  float d = mandelbrot(c + vec2(0.0, -h));

  return 0.5 * vec2(r - l, u - d) / h;
}
