/*
 * Copyright (c) 2017, Respective Authors.
 *
 * The MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#pragma once
#include <eos/chain/types.hpp>
#include <eos/chain/message.hpp>

#include <numeric>

namespace eos { namespace chain {

   /**
    * @defgroup transactions Transactions
    *
    * All transactions are sets of messages that must be applied atomically (all succeed or all fail). Transactions
    * must refer to a recent block that defines the context of the operation so that they assert a known
    * state-precondition assumed by the transaction signers.
    *
    * Rather than specify a full block number, we only specify the lower 16 bits of the block number which means you
    * can reference any block within the last 65,536 blocks which is 2.2 days with a 3 second block interval.
    *
    * All transactions must expire so that the network does not have to maintain a permanent record of all transactions
    * ever published. A transaction may not have an expiration date too far in the future because this would require
    * keeping too much transaction history in memory.
    *
    * The block prefix is the first 4 bytes of the block hash of the reference block number, which is the second 4
    * bytes of the @ref block_id_type (the first 4 bytes of the block ID are the block number)
    *
    * @note It is not recommended to set the @ref ref_block_num, @ref ref_block_prefix, and @ref expiration
    * fields manually. Call @ref set_expiration instead.
    *
    * @{
    */

   struct ProcessedTransaction;
   struct InlineTransaction;
   struct ProcessedGeneratedTransaction;

   

   /**
    * @brief methods that operate on @ref eos::types::Transaction.  
    *
    * These are utility methods for sharing common operations between inheriting types which define
    * additional features and requirements based off of @ref eos::types::Transaction.
    */
   /// Calculate the digest for a transaction
   digest_type transaction_digest(const Transaction& t);

   void transaction_set_reference_block(Transaction& t, const block_id_type& reference_block);

   bool transaction_verify_reference_block(const Transaction& t, const block_id_type& reference_block);

   template <typename T>
   void transaction_set_message(Transaction& t, int index, const types::FuncName& type, T&& value) {
      Message m(t.messages[index]);
      m.set(type, std::forward<T>(value));
      t.messages[index] = m;
   }

   template <typename T>
   T transaction_message_as(Transaction& t, int index) {
      Message m(t.messages[index]);
      return m.as<T>();
   }

   template <typename... Args>
   void transaction_emplace_message(Transaction& t, Args&&... a) {
      t.messages.emplace_back(Message(std::forward<Args>(a)...));
   }

   template <typename... Args>
   void transaction_emplace_serialized_message(Transaction& t, Args&&... a) {
      t.messages.emplace_back(types::Message(std::forward<Args>(a)...));
   }

   /**
      * clear all common data
      */
   inline void transaction_clear(Transaction& t) {
      t.messages.clear();
   }

   /**
    * @brief A generated_transaction is a transaction which was internally generated by the blockchain, typically as a
    * result of running a contract.
    *
    * When contracts run and seek to interact with other contracts, or mutate chain state, they generate transactions
    * containing messages which effect these interactions and mutations. These generated transactions are automatically
    * generated by contracts, and thus are authorized by the script that generated them rather than by signatures. The
    * generated_transaction struct records such a transaction.
    *
    * These transactions are generated while processing other transactions. The generated transactions are assigned a
    * sequential ID, then stored in the block that generated them. These generated transactions can then be included in
    * subsequent blocks by referencing this ID.
    */
   struct GeneratedTransaction : public types::Transaction {
      GeneratedTransaction() = default;
      GeneratedTransaction(generated_transaction_id_type _id, const Transaction& trx)
         : types::Transaction(trx)
         , id(_id) {}

      GeneratedTransaction(generated_transaction_id_type _id, const Transaction&& trx)
         : types::Transaction(trx)
         , id(_id) {}
      
      generated_transaction_id_type id;

      digest_type merkle_digest() const;

      typedef ProcessedGeneratedTransaction Processed;
   };

   /**
    * @brief A transaction with signatures
    *
    * signed_transaction is a transaction with an additional manifest of authorizations included with the transaction,
    * and the signatures backing those authorizations.
    */
   struct SignedTransaction : public types::SignedTransaction {
      typedef types::SignedTransaction super;
      using super::super;

      /// Calculate the id of the transaction
      transaction_id_type id()const;
      
      /// Calculate the digest used for signature validation
      digest_type         sig_digest(const chain_id_type& chain_id)const;

      /** signs and appends to signatures */
      const signature_type& sign(const private_key_type& key, const chain_id_type& chain_id);

      /** returns signature but does not append */
      signature_type sign(const private_key_type& key, const chain_id_type& chain_id)const;

      flat_set<public_key_type> get_signature_keys(const chain_id_type& chain_id)const;

      /**
       * Removes all messages, signatures, and authorizations
       */
      void clear() { transaction_clear(*this); signatures.clear(); }

      digest_type merkle_digest() const;

      typedef ProcessedTransaction Processed;
   };

   struct PendingInlineTransaction : public types::Transaction {
      typedef types::Transaction super;
      using super::super;
      
      typedef InlineTransaction Processed;
   };

   struct MessageOutput;
   struct InlineTransaction : public types::Transaction {
      explicit InlineTransaction( const PendingInlineTransaction& t ):Transaction(t){}
      InlineTransaction(){}

      vector<MessageOutput> output;
   };

   struct NotifyOutput;
   
   /**
    *  Output generated by applying a particular message.
    */
   struct MessageOutput {
      vector<NotifyOutput>             notify; ///< accounts to notify, may only be notified once
      InlineTransaction                inline_transaction; ///< transactions generated and applied after notify
      vector<GeneratedTransaction>     deferred_transactions; ///< transactions generated but not applied
   };

   struct NotifyOutput {
      AccountName   name;
      MessageOutput output;
   };

   struct ProcessedTransaction : public SignedTransaction {
      explicit ProcessedTransaction( const SignedTransaction& t ):SignedTransaction(t){}
      ProcessedTransaction(){}

      vector<MessageOutput> output;
   };

   struct ProcessedGeneratedTransaction {
      explicit ProcessedGeneratedTransaction( const generated_transaction_id_type& _id ):id(_id){}
      explicit ProcessedGeneratedTransaction( const GeneratedTransaction& t ):id(t.id){}
      ProcessedGeneratedTransaction(){}

      generated_transaction_id_type id;
      vector<MessageOutput> output;
   };
   /// @} transactions group

} } // eos::chain

FC_REFLECT(eos::chain::GeneratedTransaction, (id))
FC_REFLECT_DERIVED(eos::chain::SignedTransaction, (eos::types::SignedTransaction), )
FC_REFLECT(eos::chain::MessageOutput, (notify)(inline_transaction)(deferred_transactions) )
FC_REFLECT_DERIVED(eos::chain::ProcessedTransaction, (eos::types::SignedTransaction), (output) )
FC_REFLECT_DERIVED(eos::chain::PendingInlineTransaction, (eos::types::Transaction), )
FC_REFLECT_DERIVED(eos::chain::InlineTransaction, (eos::types::Transaction), (output) )
FC_REFLECT(eos::chain::ProcessedGeneratedTransaction, (id)(output) )
FC_REFLECT(eos::chain::NotifyOutput, (name)(output) )
