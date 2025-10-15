#include <bits/stdc++.h>
using namespace std;

enum Status {Accepted, Wrong_Answer, Runtime_Error, Time_Limit_Exceed};

static inline bool isWrong(Status s){
    return s==Wrong_Answer || s==Runtime_Error || s==Time_Limit_Exceed;
}

struct ProblemState{
    // Global across contest
    int firstAcceptTime = -1; // -1 means not yet
    int wrongBeforeAccept = 0;
    bool solved() const { return firstAcceptTime!=-1; }

    // Current freeze cycle bookkeeping
    bool preSolvedAtFreeze = false; // solved before FREEZE of current cycle
    int preFreezeWrong = 0;         // wrong attempts before FREEZE (only for unsolved-at-freeze)
    int postFreezeSubmissions = 0;  // y for display
    enum FState {None, FrozenHidden, Revealed} fstate = None;
};

struct Team{
    string name;
    vector<ProblemState> probs; // size M when START known

    // Cached visible metrics (w.r.t. current freeze/unfreeze states)
    int visSolved = 0;
    long long visPenalty = 0;
    vector<int> visSolveTimesDesc; // descending

    // For fast check: does team currently have any FrozenHidden problem?
    int frozenHiddenCount = 0;

    // Fast QUERY_SUBMISSION support
    struct LastItem { int pid=-1; Status st=Accepted; int time=-1; bool has=false; };
    LastItem lastAny;
    vector<LastItem> lastByProblem;           // size M
    array<LastItem,4> lastByStatus;           // by status
    vector<array<LastItem,4>> lastByProblemStatus; // size M x 4
};

struct Contest{
    bool started=false;
    int duration=0, M=0;
    bool frozen=false; // between FREEZE and end of SCROLL
    int freezeStartTime=0; // not strictly needed
    vector<string> problemNames; // 'A'.. but keep for iteration

    unordered_map<string,int> nameToId;
    vector<Team> teams;

    // Last flushed ranking (vector of team ids in order best->worst)
    vector<int> lastRanking;
    bool hasFlushed=false; // if false, ranking is lex by name
};

static Contest G;

static Status parseStatus(const string &s){
    if(s=="Accepted") return Accepted;
    if(s=="Wrong_Answer") return Wrong_Answer;
    if(s=="Runtime_Error") return Runtime_Error;
    // default
    return Time_Limit_Exceed;
}

static void ensureTeamProbsSized(){
    for(auto &t: G.teams){
        if((int)t.probs.size()<G.M) t.probs.assign(G.M, ProblemState());
        // Initialize last submission caches when M is known
        t.lastByProblem.assign(G.M, Team::LastItem());
        t.lastByProblemStatus.assign(G.M, array<Team::LastItem,4>());
    }
}

static void recomputeVisibleForTeam(Team &t){
    t.visSolved = 0;
    t.visPenalty = 0;
    t.visSolveTimesDesc.clear();
    for(int i=0;i<G.M;i++){
        const auto &p = t.probs[i];
        bool visible = (p.fstate==ProblemState::None || p.fstate==ProblemState::Revealed);
        if(visible && p.solved()){
            t.visSolved++;
            t.visPenalty += 20LL * p.wrongBeforeAccept + p.firstAcceptTime;
            t.visSolveTimesDesc.push_back(p.firstAcceptTime);
        }
    }
    sort(t.visSolveTimesDesc.begin(), t.visSolveTimesDesc.end(), greater<int>());
}

struct RankCmp{
    bool operator()(int a, int b) const{
        const Team &ta = G.teams[a];
        const Team &tb = G.teams[b];
        if(ta.visSolved != tb.visSolved) return ta.visSolved > tb.visSolved; // more is better
        if(ta.visPenalty != tb.visPenalty) return ta.visPenalty < tb.visPenalty; // less is better
        const auto &va = ta.visSolveTimesDesc;
        const auto &vb = tb.visSolveTimesDesc;
        // same length because visSolved equal
        for(size_t i=0;i<va.size();++i){
            if(va[i]!=vb[i]) return va[i] < vb[i]; // smaller max earlier wins
        }
        if(ta.name != tb.name) return ta.name < tb.name; // lexicographically smaller wins
        return a < b; // ensure strict weak ordering unique
    }
};

static void computeAllVisible(){
    for(auto &t: G.teams) recomputeVisibleForTeam(t);
}

static vector<int> buildRankingOrder(){
    vector<int> ids(G.teams.size());
    iota(ids.begin(), ids.end(), 0);
    sort(ids.begin(), ids.end(), RankCmp());
    return ids;
}

static void setLastRankingLex(){
    vector<int> ids(G.teams.size());
    iota(ids.begin(), ids.end(), 0);
    sort(ids.begin(), ids.end(), [](int a, int b){
        if(G.teams[a].name != G.teams[b].name) return G.teams[a].name < G.teams[b].name;
        return a < b;
    });
    G.lastRanking = move(ids);
}

static void performFlush(bool emitInfo){
    computeAllVisible();
    G.lastRanking = buildRankingOrder();
    G.hasFlushed = true;
    if(emitInfo) cout << "[Info]Flush scoreboard.\n";
}

static void printScoreboardLine(const Team &t, int ranking){
    // team_name ranking solved_count total_penalty A B C ...
    cout << t.name << ' ' << ranking << ' ' << t.visSolved << ' ' << t.visPenalty;
    for(int i=0;i<G.M;i++){
        const auto &p = t.probs[i];
        if(p.fstate==ProblemState::FrozenHidden){
            int x = p.preFreezeWrong;
            int y = p.postFreezeSubmissions;
            if(x==0) cout << ' ' << "0/" << y;
            else cout << ' ' << '-' << x << '/' << y;
        }else{
            if(p.solved()){
                int x = p.wrongBeforeAccept;
                if(x==0) cout << ' ' << '+';
                else cout << ' ' << '+' << x;
            }else{
                // not solved and not frozen
                // number of incorrect attempts total so far (since no post-freeze submissions if frozen would be true)
                int wrong = 0;
                wrong = p.wrongBeforeAccept; // wrongs before an accept; but since no accept yet, this equals total wrongs
                if(wrong==0) cout << ' ' << '.';
                else cout << ' ' << '-' << wrong;
            }
        }
    }
    cout << '\n';
}

static void printScoreboardByOrder(const vector<int> &order){
    for(size_t i=0;i<order.size();++i){
        int id = order[i];
        printScoreboardLine(G.teams[id], (int)i+1);
    }
}

static void addTeam(const string &name){
    if(G.started){ cout << "[Error]Add failed: competition has started.\n"; return; }
    if(G.nameToId.count(name)){ cout << "[Error]Add failed: duplicated team name.\n"; return; }
    Team t; t.name = name; t.probs.assign(G.M, ProblemState());
    G.nameToId[name] = (int)G.teams.size();
    G.teams.push_back(move(t));
    cout << "[Info]Add successfully.\n";
}

static void startCompetition(int duration, int problemCount){
    if(G.started){ cout << "[Error]Start failed: competition has started.\n"; return; }
    G.started = true; G.duration = duration; G.M = problemCount; G.problemNames.clear();
    for(int i=0;i<G.M;i++) G.problemNames.push_back(string(1, char('A'+i)));
    ensureTeamProbsSized();
    cout << "[Info]Competition starts.\n";
    // initial ranking is lex order of names
    setLastRankingLex();
}

static void submitEvent(char problemChar, const string &teamName, Status st, int time){
    int tid = G.nameToId[teamName];
    int pid = problemChar - 'A';
    Team &t = G.teams[tid];
    ProblemState &p = t.probs[pid];

    // Record wrong/accept effect globally
    if(p.firstAcceptTime==-1){
        if(st==Accepted){
            p.firstAcceptTime = time;
        }else if(isWrong(st)){
            p.wrongBeforeAccept += 1;
        }
    } // else already solved -> no change to wrongBeforeAccept

    if(G.frozen){
        // Determine pre-solved at freeze
        if(!p.preSolvedAtFreeze){
            // if solved before freeze, fstate should be None; else eligible
        }
        if(!p.preSolvedAtFreeze){
            // Only problems unsolved at freeze can become frozen
            if(p.fstate==ProblemState::None){
                // First post-freeze submission makes it frozen hidden
                p.fstate = ProblemState::FrozenHidden;
                t.frozenHiddenCount++;
            }
            if(p.fstate==ProblemState::FrozenHidden){
                p.postFreezeSubmissions++;
            }
        }
    }
    // When not frozen, nothing extra to do
}

static void doFlushCommand(){
    performFlush(true);
}

static void doFreezeCommand(){
    if(G.frozen){ cout << "[Error]Freeze failed: scoreboard has been frozen.\n"; return; }
    G.frozen = true;
    // Initialize per-problem freeze bookkeeping
    for(auto &t: G.teams){
        t.frozenHiddenCount = 0;
        for(int i=0;i<G.M;i++){
            auto &p = t.probs[i];
            p.postFreezeSubmissions = 0;
            if(p.solved()){
                p.preSolvedAtFreeze = true;
                p.preFreezeWrong = 0;
                p.fstate = ProblemState::None;
            }else{
                p.preSolvedAtFreeze = false;
                p.preFreezeWrong = p.wrongBeforeAccept;
                p.fstate = ProblemState::None; // not yet frozen until any post-freeze submission happens
            }
        }
    }
    cout << "[Info]Freeze scoreboard.\n";
}

static void doScrollCommand(){
    if(!G.frozen){ cout << "[Error]Scroll failed: scoreboard has not been frozen.\n"; return; }

    // First flush to have accurate ranking, then output pre-scroll scoreboard
    performFlush(false);
    cout << "[Info]Scroll scoreboard.\n";
    printScoreboardByOrder(G.lastRanking);

    // Prepare ordered sets for dynamic ranking during scroll
    computeAllVisible();
    vector<int> ids(G.teams.size()); iota(ids.begin(), ids.end(), 0);
    struct Cmp {
        bool operator()(int a, int b) const { return RankCmp()(a,b); }
    };
    std::set<int, Cmp> rankSet(ids.begin(), ids.end());

    auto hasFrozenHidden = [&](int tid)->bool{
        return G.teams[tid].frozenHiddenCount>0;
    };
    std::set<int, Cmp> frozenSet; // teams with any FrozenHidden problems
    for(int tid: ids){ if(hasFrozenHidden(tid)) frozenSet.insert(tid); }

    auto smallestFrozenProblem = [&](Team &t)->int{
        for(int i=0;i<G.M;i++) if(t.probs[i].fstate==ProblemState::FrozenHidden) return i;
        return -1;
    };

    // Iterate unfreezing until no frozen problems remain
    while(!frozenSet.empty()){
        // pick lowest-ranked team among those with frozen problems: last element
        auto itLowest = prev(frozenSet.end());
        int tid = *itLowest;
        Team &t = G.teams[tid];

        // Save neighbors around old position
        auto itOld = rankSet.find(tid);
        int oldPred = -1, oldSucc = -1;
        if(itOld!=rankSet.begin()){
            auto itp = prev(itOld); oldPred = *itp;
        }
        auto its = next(itOld);
        if(its!=rankSet.end()) oldSucc = *its;

        // Unfreeze smallest problem for this team
        int pid = smallestFrozenProblem(t);
        if(pid==-1){
            // Should not happen, but remove from frozenSet
            frozenSet.erase(itLowest);
            continue;
        }
        // Reveal it
        ProblemState &p = t.probs[pid];
        if(p.fstate==ProblemState::FrozenHidden){
            p.fstate = ProblemState::Revealed;
            t.frozenHiddenCount--;
        }

        // Recompute this team's visible stats
        rankSet.erase(itOld);
        frozenSet.erase(itLowest);
        recomputeVisibleForTeam(t);
        auto itNew = rankSet.insert(tid).first;
        if(hasFrozenHidden(tid)) frozenSet.insert(tid);

        // Determine if ranking changed
        int newPred = -1, newSucc = -1;
        if(itNew!=rankSet.begin()){
            auto itp = prev(itNew); newPred = *itp;
        }
        auto its2 = next(itNew);
        if(its2!=rankSet.end()) newSucc = *its2;

        bool changed = (oldPred!=newPred) || (oldSucc!=newSucc);
        if(changed){
            // Output: team_name1 team_name2 solved_number penalty_time
            string team1 = t.name;
            string team2 = (newSucc==-1? string("") : G.teams[newSucc].name);
            cout << team1 << ' ' << team2 << ' ' << t.visSolved << ' ' << t.visPenalty << "\n";
        }
    }

    // Lift frozen state and reset markers
    G.frozen = false;
    for(auto &t: G.teams){
        for(int i=0;i<G.M;i++){
            auto &p = t.probs[i];
            if(p.fstate==ProblemState::Revealed) p.fstate = ProblemState::None;
            p.preSolvedAtFreeze = false;
            p.preFreezeWrong = 0;
            p.postFreezeSubmissions = 0;
        }
        t.frozenHiddenCount = 0;
    }

    // Final scoreboard after scrolling
    performFlush(false);
    printScoreboardByOrder(G.lastRanking);
}

static void doQueryRanking(const string &teamName){
    auto it = G.nameToId.find(teamName);
    if(it==G.nameToId.end()){
        cout << "[Error]Query ranking failed: cannot find the team.\n";
        return;
    }
    int tid = it->second;
    cout << "[Info]Complete query ranking.\n";
    if(G.frozen){
        cout << "[Warning]Scoreboard is frozen. The ranking may be inaccurate until it were scrolled.\n";
    }
    // Determine ranking from lastRanking or lex if never flushed
    vector<int> order;
    if(G.hasFlushed) order = G.lastRanking; else order = G.lastRanking; // lastRanking initialized to lex on START
    int rankPos = -1;
    for(size_t i=0;i<order.size();++i){ if(order[i]==tid){ rankPos=(int)i+1; break; } }
    cout << teamName << " NOW AT RANKING " << rankPos << "\n";
}

static void doQuerySubmission(const string &teamName, const string &probFilter, const string &statusFilter){
    auto it = G.nameToId.find(teamName);
    if(it==G.nameToId.end()){
        cout << "[Error]Query submission failed: cannot find the team.\n";
        return;
    }
    int tid = it->second;
    // We did not store full submission history per problem (only aggregates),
    // but the requirement allows us to query last submission including during freeze.
    // Therefore, we will maintain a compact history list per team overall.
    // To satisfy, we create an auxiliary static per-team log lazily.
}

// We'll still keep a minimal global log declaration removed; now O(1) per query via caches

static void completeQuerySubmission(const string &teamName, const string &probFilter, const string &statusFilter){
    auto it = G.nameToId.find(teamName);
    if(it==G.nameToId.end()){
        cout << "[Error]Query submission failed: cannot find the team.\n";
        return;
    }
    int tid = it->second;
    bool anyProb = (probFilter=="ALL");
    int pidFilter = anyProb ? -1 : (probFilter[0]-'A');
    bool anyStatus = (statusFilter=="ALL");
    Status stWanted = anyStatus ? Accepted : parseStatus(statusFilter);

    const Team &t = G.teams[tid];
    Team::LastItem item;
    if(anyProb && anyStatus){
        item = t.lastAny;
    }else if(!anyProb && anyStatus){
        if(pidFilter>=0 && pidFilter<G.M) item = t.lastByProblem[pidFilter];
        else item.has = false;
    }else if(anyProb && !anyStatus){
        item = t.lastByStatus[(int)stWanted];
    }else{
        if(pidFilter>=0 && pidFilter<G.M) item = t.lastByProblemStatus[pidFilter][(int)stWanted];
        else item.has = false;
    }
    cout << "[Info]Complete query submission.\n";
    if(!item.has){
        cout << "Cannot find any submission.\n";
        return;
    }
    cout << t.name << ' ' << char('A'+item.pid) << ' ';
    switch(item.st){
        case Accepted: cout << "Accepted"; break;
        case Wrong_Answer: cout << "Wrong_Answer"; break;
        case Runtime_Error: cout << "Runtime_Error"; break;
        case Time_Limit_Exceed: cout << "Time_Limit_Exceed"; break;
    }
    cout << ' ' << item.time << "\n";
}

static void handleSubmitAndLog(char problemChar, const string &teamName, Status st, int time){
    int tid = G.nameToId[teamName];
    int pid = problemChar - 'A';
    submitEvent(problemChar, teamName, st, time);
    // Update fast query caches
    Team &t = G.teams[tid];
    Team::LastItem li; li.pid = pid; li.st = st; li.time = time; li.has = true;
    t.lastAny = li;
    if(pid>=0 && pid<G.M){
        t.lastByProblem[pid] = li;
        t.lastByProblemStatus[pid][(int)st] = li;
    }
    t.lastByStatus[(int)st] = li;
}

int main(){
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    string cmd;
    // We'll initialize lastRanking to empty; on START we set lex order
    while(cin>>cmd){
        if(cmd=="ADDTEAM"){
            string name; cin>>name; addTeam(name);
        }else if(cmd=="START"){
            string kw1, kw2; int duration, probCount; // DURATION x PROBLEM y
            cin>>kw1>>duration>>kw2>>probCount;
            startCompetition(duration, probCount);
        }else if(cmd=="SUBMIT"){
            string prob; string by, team, with, statusStr, at; int t;
            cin>>prob>>by>>team>>with>>statusStr>>at>>t;
            handleSubmitAndLog(prob[0], team, parseStatus(statusStr), t);
        }else if(cmd=="FLUSH"){
            doFlushCommand();
        }else if(cmd=="FREEZE"){
            doFreezeCommand();
        }else if(cmd=="SCROLL"){
            doScrollCommand();
        }else if(cmd=="QUERY_RANKING"){
            string team; cin>>team; doQueryRanking(team);
        }else if(cmd=="QUERY_SUBMISSION"){
            string team, where, probEq, statusEq; cin>>team>>where>>probEq>>statusEq;
            // probEq like PROBLEM=A or PROBLEM=ALL; statusEq like AND STATUS=Accepted
            // statusEq token includes AND, so actually we read two tokens
            if(probEq.rfind("PROBLEM=",0)==0){
                string probVal = probEq.substr(8);
                string ANDtok = statusEq; // this is actually AND
                string statusTok; cin>>statusTok; // STATUS=...
                if(statusTok.rfind("STATUS=",0)==0){
                    string statusVal = statusTok.substr(7);
                    completeQuerySubmission(team, probVal, statusVal);
                }else{
                    // malformed per spec should not happen
                }
            }
        }else if(cmd=="END"){
            cout << "[Info]Competition ends.\n";
            break;
        }else{
            // ignore unknown
        }
    }
    return 0;
}
