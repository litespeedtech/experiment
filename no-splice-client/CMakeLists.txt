cmake_minimum_required(VERSION 2.6.0)

project(client C)

add_executable(client no-splice-client.c ../client.c)
