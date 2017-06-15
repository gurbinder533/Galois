#include "Verilog.h"
#include "FileReader.h"

VerilogModule::~VerilogModule() {
  for (auto item: inputs) {
     delete item.second;
  }

  for (auto item: outputs) {
    delete item.second;
  }

  for (auto item: gates) {
    delete item.second;
  }

  for (auto item: wires) {
    delete item.second;
  }
}

VerilogModule::VerilogModule(std::string inName, CellLib& lib)
  :cellLib(lib)
{
  char delimiters[] = {
    '(', ')',
    ',', ':', ';', 
    '/',
    '#',
    '[', ']', 
    '{', '}',
    '*',
    '\"', '\\'
  };

  char separators[] = {
    ' ', '\t', '\n', ','
  };

  FileReader fRd(inName, delimiters, sizeof(delimiters), separators, sizeof(separators));

  for (std::string token = fRd.nextToken(); token != ""; token = fRd.nextToken()) {
    // module moduleName(port1, port2, ...);
    if (token == "module") {
      name = fRd.nextToken();
      do {
        token = fRd.nextToken();
      } while (token != ";");
    }

    // input/output port1, port2, ..., portN;
    else if (token == "input" || token == "output") {
      auto& primary = (token == "input") ? inputs : outputs;
      for (token = fRd.nextToken(); token != ";"; token = fRd.nextToken()) {
        VerilogPin *pin = new VerilogPin;
        pin->name = token;
        pin->gate = nullptr;
        pin->wire = nullptr;
        primary.insert({token, pin});
      }
    }

    // wire wire1, wire2, ..., wireN;
    else if (token == "wire") {
      for (token = fRd.nextToken(); token != ";"; token = fRd.nextToken()) {
        VerilogWire *wire = new VerilogWire;
        wire->name = token;
        wire->root = nullptr;
        wire->wireLoad = cellLib.defaultWireLoad;
        wires.insert({token, wire});
      }
    }

    else if (token == "endmodule") {
      break;
    }

    // logic gates: gateType gateName ( .port1 (wire1), .port2 (wire2), ... .portN (wireN) );
    else {
      VerilogGate *gate = new VerilogGate;
      gate->cell = cellLib.cells.at(token);
      gate->name = fRd.nextToken();
      gates.insert({gate->name, gate});
      fRd.nextToken(); // get "("

      // get pins and wire connections
      for (token = fRd.nextToken(); token != ")"; token = fRd.nextToken()) {
        if (token[0] != '.') {
          std::cerr << "Error: expecting .pinName(wrieName)" << std::endl;
          std::abort();
        }
        // .pinName (wireName)
        VerilogPin *pin = new VerilogPin;
        pin->name = token.substr(1);
        pin->gate = gate;

        fRd.nextToken(); // get "("
        pin->wire = wires.at(fRd.nextToken());
        fRd.nextToken(); // get ")"

        auto cellPin = gate->cell->cellPins.at(pin->name);
        if (cellPin->pinType == PIN_OUTPUT) {
          pin->wire->root = pin;
          gate->outPins.insert(pin);
        }
        else if (cellPin->pinType == PIN_INPUT) {
          pin->wire->leaves.insert(pin);
          gate->inPins.insert(pin);
        }
      }
      fRd.nextToken(); // get ";"
    }
  } // end for token

  // connect input to input wire
  for (auto item: inputs) {
    auto i = item.second;
    auto wire = wires.at(i->name);
    i->wire = wire;
    wire->root = i;
  }
  // connect output to output wire
  for (auto item: outputs) {
    auto i = item.second;
    auto wire = wires.at(i->name);
    i->wire = wire;
    wire->leaves.insert(i);
  }
}

void VerilogModule::printVerilogModuleDebug() {
  std::cout << "module " << name << std::endl;
  for (auto item: inputs) {
    auto i = item.second;
    std::cout << "input " << i->name << std::endl;
  }

  for (auto item: outputs) {
    auto o = item.second;
    std::cout << "output " << o->name << std::endl;
  }

  for (auto item: wires) {
    auto w = item.second;
    std::cout << "wire " << w->name << ": from ";
    // input/output wires don't have gates
    if (w->root->gate) {
      std::cout << w->root->gate->cell->name << ".";
    }
    std::cout << w->root->name << " to ";

    for (auto p: w->leaves) {
      // input/output wires don't have gates
      if (p->gate) {
        std::cout << p->gate->cell->name << ".";
      }
      std::cout << p->name << " ";
    }
    std::cout << std::endl;
  }

  for (auto item: gates) {
    auto g = item.second;
    std::cout << "gate: " << g->cell->name << " " << g->name << "(";
    for (auto p: g->inPins) {
      std::cout << "." << p->name << " (" << p->wire->name << ") ";
    }
    for (auto p: g->outPins) {
      std::cout << "." << p->name << " (" << p->wire->name << ") ";
    }
    std::cout << ");" << std::endl;
  }
}

