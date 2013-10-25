/*    Copyright 2013 10gen Inc.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#pragma once

#include <vector>

#include "mongo/base/disallow_copying.h"
#include "mongo/db/auth/privilege.h"
#include "mongo/db/auth/role_name.h"
#include "mongo/db/auth/user_name.h"
#include "mongo/platform/atomic_word.h"
#include "mongo/platform/unordered_map.h"
#include "mongo/platform/unordered_set.h"

namespace mongo {

    /**
     * Represents a MongoDB user.  Stores information about the user necessary for access control
     * checks and authentications, such as what privileges this user has, as well as what roles
     * the user belongs to.
     *
     * Every User object is owned by an AuthorizationManager.  The AuthorizationManager is the only
     * one that should construct, modify, or delete a User object.  All other consumers of User must
     * use only the const methods.  The AuthorizationManager is responsible for maintaining the
     * reference count on all User objects it gives out and must not mutate any User objects with
     * a non-zero reference count (except to call invalidate()).  Any consumer of a User object
     * should check isInvalidated() before using it, and if it has been invalidated, it should
     * return the object to the AuthorizationManager and fetch a new User object instance for this
     * user from the AuthorizationManager.
     */
    class User {
        MONGO_DISALLOW_COPYING(User);
    public:
        struct CredentialData {
            std::string password;
            bool isExternal;
        };

        explicit User(const UserName& name);
        ~User();

        /**
         * Returns the user name for this user.
         */
        const UserName& getName() const;

        /**
         * Returns an iterator that can be used to get the list of roles this user belongs to.
         */
        const RoleNameIterator getRoles() const;

        /**
         * Gets the set of actions this user is allowed to perform on the given resource.
         */
        const ActionSet getActionsForResource(const std::string& resource) const;

        /**
         * Gets the schema version of user documents used to build this user.  See comment on
         * _schemaVersion field, below.
         */
        int getSchemaVersion() const { return _schemaVersion; }

        /**
         * Returns true if this user object, generated from V1-schema user documents,
         * has been probed for privileges on database "dbname", according to the V1
         * implicit privilge acquisition rules.
         */
        bool hasProbedV1(const StringData& dbname) const;

        /**
         * Returns true if this copy of information about this user is still valid. If this returns
         * false, this object should no longer be used and should be returned to the
         * AuthorizationManager and a new User object for this user should be requested.
         */
        bool isValid() const;

        /**
         * This returns the reference count for this User.  The AuthorizationManager should be the
         * only caller of this.
         */
        uint32_t getRefCount() const;


        // Mutators below.  Mutation functions should *only* be called by the AuthorizationManager

        /**
         * Copies the contents of other into this User.
         */
        void copyFrom(const User& other);

        /**
         * Sets this user's authentication credentials.
         */
        void setCredentials(const CredentialData& credentials);

        /**
         * Adds the given role name to the list of roles of which this user is a member.
         */
        void addRole(const RoleName& role);

        /**
         * Adds the given role names to the list of roles that this user belongs to.
         */
        void addRoles(const std::vector<RoleName>& roles);

        /**
         * Adds the given privilege to the list of privileges this user is authorized for.
         */
        void addPrivilege(const Privilege& privilege);

        /**
         * Adds the given privileges to the list of privileges this user is authorized for.
         */
        void addPrivileges(const PrivilegeVector& privileges);

        /**
         * Sets the schema version of documents used for building this user to 1, for V1 and V0
         * documents.  The default value is 2, for V2 documents.
         */
        void setSchemaVersion1();

        /**
         * Marks that this user object, generated from V1-schema user documents,
         * has been probed for privileges on database "dbname", according to the V1
         * implicit privilge acquisition rules.
         */
        void markProbedV1(const StringData& dbname);

        /**
         * Marks this instance of the User object as invalid, most likely because information about
         * the user has been updated and needs to be reloaded from the AuthorizationManager.
         *
         * This method should *only* be called by the AuthorizationManager.
         */
        void invalidate();

        /**
         * Increments the reference count for this User object, which records how many threads have
         * a reference to it.
         *
         * This method should *only* be called by the AuthorizationManager.
         */
        void incrementRefCount();

        /**
         * Decrements the reference count for this User object, which records how many threads have
         * a reference to it.  Once the reference count goes to zero, the AuthorizationManager is
         * allowed to destroy this instance.
         *
         * This method should *only* be called by the AuthorizationManager.
         */
        void decrementRefCount();

    private:

        UserName _name;

        typedef unordered_map<std::string, Privilege> ResourcePrivilegeMap;

        // Maps resource name to privilege on that resource
        ResourcePrivilegeMap _privileges;

        unordered_set<RoleName> _roles;

        // List of databases already probed for privilege information for this user.  Only
        // meaningful for V1-schema users.
        std::vector<std::string> _probedDatabases;

        // Credential information.
        CredentialData _credentials;

        // Schema version of user documents used to build this user.  Valid values are 1 (for V1 and
        // V0 documents) and 2 (for V2 documents).  We need this information because the V1 and V0
        // users need to do extra probing when checking for privileges.  See
        // AuthorizationManager::updateV1UserForResource().  Defaults to 2.
        int _schemaVersion;

        // _refCount and _isInvalidated are modified exclusively by the AuthorizationManager
        // _isInvalidated can be read by any consumer of User, but _refCount can only be
        // meaningfully read by the AuthorizationManager, as _refCount is guarded by the AM's _lock
        uint32_t _refCount;
        AtomicUInt32 _isValid; // Using as a boolean
    };

} // namespace mongo
