/* SPDX-License-Identifier: MIT */
/* Definicion propia del header de exportacion que CMake genera aguas arriba
   con generate_export_header. miniz se compila embebido en los targets del
   motor (biblioteca estatica / objetos), sin exportar simbolos, por lo que la
   marca de exportacion queda vacia en todas las plataformas. */
#ifndef MINIZ_EXPORT_H
#define MINIZ_EXPORT_H

#define MINIZ_EXPORT
#define MINIZ_NO_EXPORT

#endif /* MINIZ_EXPORT_H */
