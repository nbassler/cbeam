#include <iostream>
#include "model.h"
#include "ui_slider.h"

Model::Model() {
    pos_step = 0;
    pos_mm = 0;
    mm_per_step = 40.0 / 1000.0; // mm per step
}

void Model::aaa() {
    std::cout << "hey ho aaa\n";
}

void Model::zero() {
    std::cout << "hey ho bbb\n";
}

void Model::go() {
    std::cout << "hey ho sss\n";
}

void Model::set_step(int var) {
}

void Model::set_llim(int var) {
}

void Model::set_ulim(int var) {
}
