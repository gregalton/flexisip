/*
    Flexisip, a flexible SIP proxy server with media capabilities.
    Copyright (C) 2010-2025 Belledonne Communications SARL, All rights reserved.

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#include "flexisip/fork-context/fork-context.hh"

#include "agent.hh"
#include "branch-info.hh"
#include "eventlogs/writers/event-log-writer.hh"
#include "transaction/incoming-transaction.hh"
#include "transaction/outgoing-transaction.hh"

using namespace std;
using namespace flexisip;

shared_ptr<ForkContext> ForkContext::getFork(const shared_ptr<IncomingTransaction>& tr) {
	return tr->getProperty<ForkContext>("ForkContext");
}

shared_ptr<ForkContext> ForkContext::getFork(const shared_ptr<OutgoingTransaction>& tr) {
	shared_ptr<BranchInfo> br = BranchInfo::getBranchInfo(tr);
	return br ? br->mForkCtx.lock() : nullptr;
}

void ForkContext::setFork(const shared_ptr<IncomingTransaction>& tr, const shared_ptr<ForkContext>& fork) {
	tr->setProperty<ForkContext>("ForkContext", weak_ptr<ForkContext>{fork});
}

void ForkContext::processCancel(const shared_ptr<RequestSipEvent>& ev) {
	auto transaction = dynamic_pointer_cast<IncomingTransaction>(ev->getIncomingAgent());

	if (transaction && ev->getMsgSip()->getSip()->sip_request->rq_method == sip_method_cancel) {
		auto ctx = ForkContext::getFork(transaction);

		if (ctx) {
			ctx->onCancel(ev);
			ev->terminateProcessing();
		}
	}
}

bool ForkContext::processResponse(const shared_ptr<ResponseSipEvent>& ev) {
	auto transaction = dynamic_pointer_cast<OutgoingTransaction>(ev->getOutgoingAgent());
	if (transaction) {
		auto bInfo = BranchInfo::getBranchInfo(transaction);
		if (bInfo) {
			auto forkCtx = bInfo->mForkCtx.lock();
			if (!forkCtx) {
				ev->terminateProcessing();
				return true;
			}

			// Sofia delivers CANCEL responses on the same nta_outgoing_t as the INVITE
			// (nta_outgoing_tcancel). Those responses must never be treated as answers to
			// the forked INVITE: forwarding them through the INVITE IncomingTransaction
			// trips nta_incoming_mreply's cs_method == irq_method assert (SIGABRT).
			// Seen live on flexisip-v2: 481 to CANCEL forwarded on INVITE irq → abort.
			const auto* responseSip = ev->getMsgSip() ? ev->getMsgSip()->getSip() : nullptr;
			const auto& forkEvent = forkCtx->getEvent();
			const auto* requestSip = forkEvent && forkEvent->getMsgSip() ? forkEvent->getMsgSip()->getSip() : nullptr;
			if (responseSip && responseSip->sip_cseq && requestSip && requestSip->sip_request &&
			    responseSip->sip_cseq->cs_method != requestSip->sip_request->rq_method) {
				const auto* cseqName = responseSip->sip_cseq->cs_method_name
				                           ? responseSip->sip_cseq->cs_method_name
				                           : "?";
				const auto* reqName = requestSip->sip_request->rq_method_name
				                          ? requestSip->sip_request->rq_method_name
				                          : "?";
				const auto status =
				    responseSip->sip_status ? responseSip->sip_status->st_status : 0;
				SLOGW << "ForkContext: dropping response CSeq method '" << cseqName << "' (" << status
				      << ") that does not match forked request method '" << reqName
				      << "' — not storing on branch and not forwarding to incoming transaction";
				ev->terminateProcessing();
				return true;
			}

			auto copyEv = make_shared<ResponseSipEvent>(ev); // make a copy
			copyEv->suspendProcessing();
			bInfo->mLastResponse = copyEv;

			forkCtx->onResponse(bInfo, copyEv);

			// The fork has taken ownership of this response via copyEv / mLastResponse.
			// Terminate the original event unconditionally so it cannot leak through
			// ForwardModule — even when copyEv is held suspended on the branch (e.g.
			// a 503/408 held for a fork-late re-dispatch).  The retained response will
			// be forwarded later by ForkContextBase::forwardResponse() when the fork
			// closes or a better branch answers.
			ev->terminateProcessing();

			if (forkCtx->allCurrentBranchesAnswered(FinalStatusMode::RFC) && forkCtx->hasNextBranches()) {
				forkCtx->start();
			}

			return true;
		} else {
			// LOGD("ForkContext: un-processed response");
		}
	}

	return false;
}

std::string ForkContext::errorLogPrefix() const {
	std::stringstream prefix;
	prefix << this->getClassName() << "[" << this << "] - fork error - ";
	return prefix.str();
}

std::string ForkContext::logPrefix() const {
	std::stringstream prefix;
	prefix << this->getClassName() << "[" << this << "] - ";
	return prefix.str();
}
