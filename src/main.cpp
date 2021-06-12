#include "flow.h"
#include "binaryImporter.h"

#include <iostream>
#include <sstream>

using namespace std;

DB db;
extern "C"
{

size_t loadDbFromFile(char const *_filename) {
    ifstream input_file(_filename);
    if (!input_file.is_open()) {
        cerr << "Could not open '" << _filename << "'" << endl;
        return 0;
    }

    size_t blockNumber{};
    tie(blockNumber, db) = BinaryImporter(input_file).readBlockNumberAndDB();

    input_file.close();

    return blockNumber;
}

size_t loadDB(char const *_data, size_t _length) {
    string data(_data, _length);
    istringstream stream(data);

    size_t blockNumber{};
    tie(blockNumber, db) = BinaryImporter(stream).readBlockNumberAndDB();

    return blockNumber;
}

Flow computeFlow(
        Address const &_source,
        Address const &_sink,
        Int const &_value
) {

    cout << "Edges: " << db.m_edges.size() << endl;

    auto[flow, transfers] = computeFlow(_source, _sink, db.edges(), _value);

    cout << "Max flow: " << to_string(flow) << endl;

    return Flow(flow, transfers);
}

size_t edgeCount() {
    return db.edges().size();
}

void delayEdgeUpdates() {
    cerr << "Delaying edge updates." << endl;
    db.delayEdgeUpdates();
}

void performEdgeUpdates() {
    db.performEdgeUpdates();
}

TrustRelation* adjacencies(string const& _user)
{
    Address user{string(_user)};

    cout << "Adjacencies of " << _user << ":" << endl;
    cout << "------------------------" << endl;
    auto v = new vector<TrustRelation>();
    for (auto const& [address, safe]: db.safes) {
        for (auto const&[sendTo, percentage]: safe.limitPercentage) {
            if (sendTo != address && (user == address || user == sendTo)) {
                auto a = TrustRelation(
                        sendTo,
                        user == sendTo ? address : user,
                        percentage);

                v->push_back(a);
                cout << "TrustRelation from " << a.from << " to " << a.to << " with limit " << a.limit << endl;
            }
        }
    }

    return v->data();
}


void signup(char const *_user, char const *_token) {
    db.signup(Address(string(_user)), Address(string(_token)));
}

void organizationSignup(char const *_organization) {
    db.organizationSignup(Address(string(_organization)));
}

void trust(char const *_canSendTo, char const *_user, int _limitPercentage) {
    db.trust(Address(string(_canSendTo)), Address(string(_user)), uint32_t(_limitPercentage));
}

void transfer(char const *_token, char const *_from, char const *_to, Int _value) {
    db.transfer(
            Address(string(_token)),
            Address(string(_from)),
            Address(string(_to)),
            _value
    );
}
}

int main() {
    loadDbFromFile("/home/daniel/src/circles-world/libpathfinder/db.dat");
    adjacencies("0xDE374ece6fA50e781E81Aac78e811b33D16912c7");

    computeFlow(
            Address("0xDE374ece6fA50e781E81Aac78e811b33D16912c7"),
            Address("0x4a9aFfA9249F36fd0629f342c182A4e94A13C2e0"),
            Int("0"));
    computeFlow(
            Address("0x42cEDde51198D1773590311E2A340DC06B24cB37"),
            Address("0xDE374ece6fA50e781E81Aac78e811b33D16912c7"),
            Int("0"));
    computeFlow(
            Address("0xDE374ece6fA50e781E81Aac78e811b33D16912c7"),
            Address("0x42cEDde51198D1773590311E2A340DC06B24cB37"),
            Int("0"));

    return 0;
}