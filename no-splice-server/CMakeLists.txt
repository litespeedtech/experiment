cmake_minimum_required(VERSION 2.6.0)

project(server C)

add_executable(server no-splice-server.c ../server.c)
