# Security Investigations Tracker

## Completed investigations

- [x] **Aggregate function state deserialization** — 15 vulnerabilities found (1 heap overflow, 6 allocation bombs, 4 unbounded loops, 2 ForEach issues, 2 Column arena issues). Filed as #52488. QuantileTiming OOB already fixed by #47867.
- [x] **Compression codec decompression** — CompressionCodecMultiple crash (#52490), T64 OOB reads (#52491), missing `size_decompressed` bound (#52492), Gorilla UB shift (#52493).
- [x] **Compression codec compression** — DoubleDelta/Gorilla `getCompressedDataSize` integer overflow (DoS only, BitWriter catches). No issue filed.
- [x] **Credential masking in FunctionSecretArgumentsFinder** — paimonCluster/paimonS3Cluster/paimonAzureCluster/paimonHDFSCluster/deltaLakeS3 missing. Filed #52495.
- [x] **RBAC: hasColumnInTable** — No access check, allows column/table/database enumeration. Filed #52496.
- [x] **RBAC: system.asynchronous_inserts** — Cross-user metadata disclosure. Filed #52497.
- [x] **RBAC: system.query_cache** — Cross-user visibility is BY DESIGN (explicit code comment at StorageSystemQueryResultCache.cpp:55-59).
- [x] **Path traversal: scalar file() function** — `starts_with` instead of `fileOrSymlinkPathStartsWith`. Confirmed still exploitable. Comment added to existing #47741.
- [x] **Path traversal: catboostEvaluate** — No path validation, file existence oracle. Filed #52498.
- [x] **Path traversal: DataLake google_adc_credentials_file** — No path validation, reads arbitrary JSON. Filed #52499.
- [x] **Path traversal: SQLite, ATTACH TABLE, FormatSchemaInfo** — Weak `starts_with` checks but safe by accident (trailing slash in config). Not filed separately.
- [x] **Native TCP protocol: unauthenticated Hello OOM** — 4 GiB allocation pre-auth via oversized strings. Filed #52501.
- [x] **Native TCP protocol: post-auth pre-allocation limits** — 1T rows, 1M columns, 1 GiB strings. Filed #52502.
- [x] **Native TCP protocol: TablesStatus pre-auth leak** — Unauthenticated table existence probing via fake interserver connection. Filed #52505.
- [x] **Dictionary SSRF: MySQL source** — Missing `RemoteHostFilter` check for inline DDL params. Filed #52506. PostgreSQL/HTTP/ClickHouse/MongoDB/Redis/Cassandra all properly checked.
- [x] **Table function SSRF** — All table functions (remote, mysql, postgresql, url, s3, azure) properly check RemoteHostFilter. remote() requires REMOTE grant.
- [x] **HTTP handler SSRF** — URL scheme whitelist in HTTPConnectionPool blocks file://. Redirects re-validated. No issues.
- [x] **SQL parser/analyzer depth limits** — Parser depth (300), AST depth (1000), optimizer limit (10000) all properly enforced. Huge IN lists use memory but tracked.
- [x] **DoS via resource exhaustion** — All standard DoS patterns caught by `max_memory_usage`/`max_execution_time`. Dictionary loading and per-thread batching provide small untracked headroom but not exploitable.
- [x] **Zip/archive bombs** — Streaming decompression, no special threat vs plain large files. Memory tracker catches accumulation.
- [x] **Backup/restore security** — Destination path validation solid. Privilege checks on access entities correct. MISSED: file name path traversal in restore write path (#52832, found by external researcher).
- [x] **Format parsing: Parquet/ORC/Arrow** — `asArrowFileLoadIntoMemory` loads full file for non-seekable streams (tracked by memory tracker). Arrow IPC stream has 256 MiB metadata pre-validation.
- [x] **Format parsing: MsgPack** — No `unpack_limit` configured but stream exhaustion prevents OOM.
- [x] **Format parsing: Avro** — Unbounded string/bytes but stream-limited.
- [x] **Format parsing: Protobuf** — Custom reader, safe varint decoding.
- [x] **Format parsing: Cap'n Proto** — Well protected (1 GiB cap, traversal limits).
- [x] **Format parsing: BSON** — Skip path is iterative, read path bounded by schema depth.
- [x] **Format parsing: Npy** — Negative shape causes infinite loop. Filed public #99585.
- [x] **Inter-server communication** — Requires compromised shard (already authenticated). MarkRanges::deserialize has no size limit but threat model is low.
- [x] **Settings injection** — Constraint system consistently enforced across SET/HTTP/SETTINGS. readonly cannot be bypassed. No issues.
- [x] **ZooKeeper/Keeper interaction** — ZK path hijacking for ReplicatedMergeTree confirmed but needs Cloud verification (SharedMergeTree). system.zookeeper reads arbitrary paths. Shared ZK sessions by design.
- [x] **from_env/from_zk in user SQL** — Only disk() function is vulnerable. PR #99138 fix validated. No other paths found.
- [x] **NaiveBayesClassifier model_path** — Safe. Path comes from server config, not user SQL.

## Pending investigations

### Unauthenticated (highest priority)

- [ ] **HTTP handler: comprehensive CRLF/header injection audit** — Check ALL user-controlled values in HTTP response headers beyond query_id. Search for `set(` and `add(` on Poco HTTP response objects with user data.
- [ ] **HTTP request smuggling** — Can HTTP request smuggling via the Poco HTTP server affect ClickHouse? Test with ambiguous Content-Length/Transfer-Encoding.
- [ ] **Native TCP protocol: comprehensive pre-auth audit** — Beyond Hello and TablesStatus, are there other packet types processed before authentication? Check all cases in `receivePacketsExpectQuery`.
- [ ] **HTTP native compression pre-auth** — Compressed blocks are decompressed before the query is parsed/authenticated. Can crafted blocks crash the server unauthenticated?
- [ ] **gRPC/MySQL/PostgreSQL wire protocol pre-auth** — ClickHouse supports MySQL and PostgreSQL wire protocols. Are there pre-auth vulnerabilities in those handlers?
- [ ] **Keeper protocol pre-auth** — The built-in Keeper server accepts TCP connections. What happens before session authentication?
- [ ] **Interserver HTTP handler pre-auth** — The interserver HTTP port (9009) accepts connections. What can be done without cluster secret?

### Cloud-only / private features (HIGH PRIORITY — less community scrutiny)

- [ ] **SharedMergeTree: ZK path manipulation and tenant isolation** — Does SharedMergeTree prevent one tenant from accessing another's data via crafted Keeper paths? Compare with ReplicatedMergeTree ZK path hijack (confirmed on open-source).
- [ ] **SharedMergeTree: S3 metadata injection** — Can crafted part metadata in Keeper point to another tenant's S3 objects? Check how S3 keys are constructed and validated.
- [ ] **Shared database / SharedDatabaseCatalog** — DDL replication via `SharedDatabaseCatalog::tryExecuteDDLQuery`. Can a user inject DDL that executes with elevated privileges? Check catalog isolation between tenants.
- [ ] **JWT authentication** — Server-side JWT validation (`AuthenticationType::JWT`). Check: algorithm confusion (RS256 vs HS256), token expiry bypass, claim injection, key confusion, missing audience/issuer validation. Review `src/Access/AuthenticationData.cpp` JWT path.
- [ ] **Distributed cache tenant isolation** — `DistributedCache::Registry`. Can one tenant read another's cached data? Check cache key construction for tenant ID inclusion.
- [ ] **Cloud HTTP endpoints** — Internal management endpoints, health checks. Are they properly authenticated? Check for endpoints accessible without credentials.
- [ ] **Cloud-specific settings and privileges** — Settings only in clickhouse-private. Check if any bypass standard access controls.
- [ ] **Lightweight updates/deletes** — Cloud-optimized mutation path. Can mutations be injected or corrupted?

### Authenticated (high priority)

- [ ] **RBAC: comprehensive table function audit** — Systematically verify ALL table functions check privileges on referenced tables. Priority: `merge()`, `loop()`, `view()`, `mergeTreeIndex()`, `mergeTreeProjection()`, `joinGet()`, `dictGet()`, `clusterAllReplicas()`.
- [ ] **RBAC: materialized view privilege escalation** — Verify MV queries check access on all referenced tables (known issue #40418). Test with JOINs, subqueries, table functions inside MVs.
- [ ] **RBAC: ATTACH VIEW / SQL SECURITY bypass** — Can ATTACH VIEW bypass SQL SECURITY DEFINER checks? (known pattern from #46981)
- [ ] **RBAC: Buffer engine wrapping inaccessible tables** — Can a user create a Buffer table pointing to a table they can't SELECT from? (known pattern from #37983)
- [ ] **RBAC: row policy bypass** — Systematic test of all table functions/engines that might bypass row policies.
- [ ] **HTTP header injection** — Check ALL user-controlled values that flow into HTTP response headers: `query_id`, `X-ClickHouse-*` headers, redirect URLs, `Content-Disposition`. CRLF injection via `%0d%0a` in query_id already found (#52866).
- [ ] **Backup restore path traversal (WRITE)** — The file name from `.backup` XML is used as destination path during restore via `restorePartFromBackup` line 7039. Already reported as #52832. Verify fix.
- [ ] **New aggregate functions since last audit** — Check any newly added aggregate functions for deserialization vulnerabilities.
- [ ] **New table functions/engines since last audit** — Check recently added table functions and engines for RBAC, path validation, SSRF.
- [ ] **Parquet/ORC metadata bombs** — Crafted Parquet/ORC files with huge metadata claiming enormous decompressed sizes. Arrow library trusts metadata.
- [ ] **Custom disk configuration beyond from_env** — Are there other XML substitution mechanisms reachable from user SQL?
- [ ] **ZK path hijacking on SharedMergeTree** — Verify if the ReplicatedMergeTree ZK path hijack works with SharedMergeTree in Cloud (needs private code access).
- [ ] **UDF executable sandbox escapes** — Can user-defined functions via `executable` UDFs escape the intended sandbox?
- [ ] **DNS rebinding attacks** — RemoteHostFilter checks hostname but DNS can rebind between check and connect. Test with MongoDB, S3, URL table functions.
- [ ] **Integer overflow in size calculations** — Systematic search for `size * sizeof(T)` without overflow checks in deserialization paths.
- [ ] **Stack overflow via deep recursion** — Search for recursive functions without depth limits in analyzer, planner, and column operations.
- [ ] **Race conditions in RBAC checks** — Can privileges be revoked between the check and the operation? TOCTOU in access control.
- [ ] **Encrypted codec key extraction** — Can a user craft a compressed block with encrypted codec method byte to trigger decryption with server keys?
- [ ] **system.zookeeper write access** — When `allow_zookeeper_write=true`, verify what damage can be done via writes to arbitrary ZK paths.
- [ ] **Query result cache poisoning** — Can a user poison the query cache to serve wrong results to another user? (cache key includes user but need to verify)
- [ ] **Distributed DDL injection** — Can a user inject DDL commands that execute with different privileges on remote nodes?
- [ ] **External dictionary source privilege escalation** — Dictionary source using privileged default user when USER/PASSWORD omitted (#47743). Verify fix.
- [ ] **CVEs in bundled dependencies** — All dependencies are vendored under `contrib/`. For each key library, check the pinned version (`git -C contrib/<lib> log --oneline -1`) and search for CVEs against it. Verify if vulnerable code paths are reachable from ClickHouse user input (e.g., Parquet file parsing, HTTP handling, compression). Priority libraries: `arrow`, `poco`, `openssl`/`boringssl`, `zlib-ng`, `lz4`, `zstd`, `re2`, `protobuf`, `grpc`, `curl`, `librdkafka`, `msgpack-c`, `avro`, `capnproto`, `croaring`, `hyperscan`/`vectorscan`, `libxml2`, `minizip`, `aws`.
- [ ] **HTTP pipelining/smuggling** — Can HTTP request smuggling via the Poco HTTP server affect ClickHouse?
- [ ] **WebSocket/upgrade handling** — Does ClickHouse handle HTTP upgrade requests safely?
- [ ] **Keeper protocol fuzzing** — Crafted Keeper protocol messages to the built-in Keeper server.
- [ ] **S3 response injection** — Can a malicious S3-compatible server send crafted responses that exploit ClickHouse's S3 client?
- [ ] **Connection pool reuse** — Can connection pool reuse between users leak state? (HTTP keep-alive, native protocol connection reuse)
